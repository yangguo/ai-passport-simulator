#!/usr/bin/env python3
"""Compare normalized firmware warning signatures with a reviewed allowlist."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent
ALLOWLIST_DIR = ROOT / "warning-allowlist"
WARNING_RE = re.compile(r"(?i)(?:\bwarn(?:ing)?\b|pass_with_caveats|\b[WE]\s*\(\s*\d+\s*\))")


def normalize(line: str) -> str:
    line = line.strip()
    # Remove only log timestamps and ANSI presentation escapes; warning identity remains intact.
    line = re.sub(r"\x1b\[[0-9;]*m", "", line)
    line = re.sub(r"^\[\d{2}:\d{2}:\d{2}(?:\.\d+)?\]\s*", "", line)
    return re.sub(r"\s+", " ", line)


def extract_warnings(text: str) -> list[str]:
    found = []
    for line in text.splitlines():
        if WARNING_RE.search(line):
            signature = normalize(line)
            if signature:
                found.append(signature)
    return found


def extract_status_warnings(payload: object) -> list[str]:
    found: list[str] = []
    def visit(value: object) -> None:
        if isinstance(value, dict):
            for key, child in value.items():
                normalized_key = key.lower()
                if normalized_key in {"timing_lint", "unmodeled_first_touch", "warnings", "warning", "caveats"}:
                    values = child if isinstance(child, list) else [child]
                    for item in values:
                        if isinstance(item, (str, int, float)):
                            found.append(f"{normalized_key}: {normalize(str(item))}")
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)
    visit(payload)
    return found


def extract_log_warnings(text: str) -> list[str]:
    found: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("{"):
            try:
                found.extend(extract_status_warnings(json.loads(stripped)))
                continue
            except ValueError:
                raise RuntimeError("unparseable status JSON warning stream")
        if WARNING_RE.search(line):
            signature = normalize(line)
            if signature:
                found.append(signature)
    return found


def extract_from_run(run_dir: Path) -> list[str]:
    warnings: list[str] = []
    for run_name in ("run1", "run2"):
        path = run_dir / run_name / "session.log"
        if path.is_file():
            warnings.extend(extract_log_warnings(path.read_text(encoding="utf-8", errors="replace")))
        summary_path = run_dir / run_name / "summary.json"
        if summary_path.is_file():
            try:
                summary = json.loads(summary_path.read_text(encoding="utf-8"))
            except (OSError, ValueError) as error:
                raise RuntimeError(f"unparseable run summary warning stream {summary_path}: {error}") from error
            for command in summary.get("commands", []):
                if str(command.get("accepted_as", "")).startswith("pass_with_caveats"):
                    warnings.append("pass_with_caveats: " + normalize(str(command.get("accepted_as"))))
    for path in run_dir.rglob("status*.json"):
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise RuntimeError(f"unparseable status JSON {path}: {error}") from error
        warnings.extend(extract_status_warnings(payload))
    return [item for item in warnings if item]


def read_allowlist(path: Path) -> dict[str, int]:
    allowed: dict[str, int] = {}
    if not path.is_file():
        return allowed
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.match(r"^(\d+)\t(.+)$", line)
        if match:
            allowed[match.group(2)] = int(match.group(1))
        else:
            allowed[line] = 2**31 - 1
    return allowed


def evaluate(observed: list[str], allowed: dict[str, int] | set[str]) -> dict[str, object]:
    limits = ({key: 2**31 - 1 for key in allowed} if isinstance(allowed, set) else allowed)
    counts = Counter(observed)
    new = sorted(signature for signature in counts if signature not in limits)
    excessive = sorted(signature for signature, count in counts.items()
                       if signature in limits and count > limits[signature])
    disappeared = sorted(signature for signature in limits if signature not in counts)
    return {"passed": not new and not excessive, "observed": dict(sorted(counts.items())),
            "new": new, "excessive": excessive, "disappeared": disappeared}


def write_allowlist(path: Path, observed: list[str]) -> None:
    counts = Counter(observed)
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Maximum observed count per two-run invocation, followed by exact warning signature."]
    lines.extend(f"{count}\t{signature}" for signature, count in sorted(counts.items()))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run(run_dir: Path, corpus_id: str, update: bool = False) -> dict[str, object]:
    observed = extract_from_run(run_dir)
    allowlist_path = ALLOWLIST_DIR / f"{corpus_id}.txt"
    if update:
        write_allowlist(allowlist_path, observed)
    result = evaluate(observed, read_allowlist(allowlist_path))
    result["corpus_id"] = corpus_id
    result["allowlist"] = str(allowlist_path)
    (run_dir / "warning-results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for heading in ("new", "excessive", "disappeared"):
        entries = result[heading]
        if entries:
            print(f"{heading} warnings for {corpus_id}:", file=sys.stderr)
            for entry in entries:
                print(f"  {entry}", file=sys.stderr)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus_id", choices=("gov011", "chessfd8fa19", "xxlaba64c1"))
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--update", action="store_true")
    args = parser.parse_args()
    try:
        result = run(args.run_dir.resolve(), args.corpus_id, args.update)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"check-warnings: {error}", file=sys.stderr)
        return 1
    print(f"{args.corpus_id}: {'PASS' if result['passed'] else 'FAIL'} warning gate; "
          f"{len(result['observed'])} signatures, {len(result['new'])} new")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
