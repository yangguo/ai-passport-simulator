#!/usr/bin/env python3
"""Fetch and verify the exact game firmware source pinned in firmware-sources.toml."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib
import zipfile

ROOT = Path(__file__).resolve().parent
LOCK = ROOT / "firmware-sources.toml"
ENV_NAMES = {
    "gov011": {"bin": "PASSPORTSIM_GOV011_BIN"},
    "chessfd8fa19": {"bin": "PASSPORTSIM_CHESSFD8FA19_BIN", "elf": "PASSPORTSIM_CHESSFD8FA19_ELF"},
    "xxlaba64c1": {"bin": "PASSPORTSIM_XXLABA64C1_BIN", "elf": "PASSPORTSIM_XXLABA64C1_ELF"},
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_gh(args: list[str], cwd: Path | None = None) -> str:
    result = subprocess.run(["gh", *args], cwd=cwd, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or f"gh {' '.join(args)} failed ({result.returncode})")
    return result.stdout


def member_file(root: Path, member: str) -> Path:
    matches = [path for path in root.rglob(Path(member).name) if path.is_file()]
    if len(matches) != 1:
        raise RuntimeError(f"expected one {member!r} in downloaded bundle; found {len(matches)}")
    return matches[0]


def get_run_artifact(entry: dict[str, object], scratch: Path) -> None:
    repo = str(entry["repo"])
    run_id = str(entry["run_id"])
    metadata = json.loads(run_gh(["api", f"repos/{repo}/actions/runs/{run_id}/artifacts"]))
    artifact = next((item for item in metadata.get("artifacts", [])
                     if item.get("name") == entry["artifact"]), None)
    if artifact is None:
        raise RuntimeError(f"artifact {entry['artifact']!r} is missing from {repo} run {run_id}")
    if artifact.get("expired"):
        expiry = artifact.get("expires_at", "unknown date")
        raise RuntimeError(
            f"artifact {entry['artifact']} from {repo} run {run_id} expired on {expiry}; "
            "publish/promote a new immutable firmware source and update firmware-sources.toml"
        )
    run_gh(["run", "download", run_id, "-R", repo, "-n", str(entry["artifact"]), "-D", str(scratch)])


def download(entry: dict[str, object], scratch: Path) -> Path:
    if entry["kind"] == "release":
        archive = scratch / str(entry["asset"])
        run_gh(["release", "download", str(entry["tag"]), "-R", str(entry["repo"]),
                "-p", str(entry["asset"]), "-D", str(scratch)])
        extract = scratch / "release"
        extract.mkdir()
        with zipfile.ZipFile(archive) as bundle:
            wanted = {str(entry["bin_member"])}
            if entry.get("elf_member"):
                wanted.add(str(entry["elf_member"]))
            for info in bundle.infolist():
                if Path(info.filename).name in {Path(name).name for name in wanted}:
                    destination = extract / Path(info.filename).name
                    destination.write_bytes(bundle.read(info))
        return extract
    if entry["kind"] == "run":
        get_run_artifact(entry, scratch)
        return scratch
    raise RuntimeError(f"unsupported source kind: {entry['kind']}")


def update_lock(corpus_id: str, values: dict[str, str]) -> None:
    content = LOCK.read_text(encoding="utf-8")
    start = content.index(f"[corpora.{corpus_id}]")
    next_table = content.find("\n[corpora.", start + 1)
    end = len(content) if next_table < 0 else next_table + 1
    section = content[start:end]
    for key, value in values.items():
        pattern = rf'(?m)^({re.escape(key)}\s*=\s*)"[0-9a-fA-F]*"\s*$'
        section, count = re.subn(pattern, rf'\g<1>"{value}"', section, count=1)
        if count != 1:
            raise RuntimeError(f"cannot update {key} in {corpus_id} lock entry")
    LOCK.write_text(content[:start] + section + content[end:], encoding="utf-8")


def configure(home: Path, corpus_id: str, paths: dict[str, Path], source: dict[str, object]) -> None:
    spec = importlib.util.spec_from_file_location("configure_corpus", ROOT / "configure-corpus.py")
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.configure(home, corpus_id, paths, source)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus_id", choices=ENV_NAMES)
    parser.add_argument("--home", required=True, type=Path, help="PASSPORTSIM_HOME for generated config and manifest")
    parser.add_argument("--store", type=Path, help="private firmware store (default: env or ~/.cache/apsim/firmware)")
    parser.add_argument("--update-lock", action="store_true", help="rewrite expected hashes with downloaded file hashes")
    args = parser.parse_args()
    with LOCK.open("rb") as stream:
        lock = tomllib.load(stream)["corpora"]
    entry = lock[args.corpus_id]
    override_id = os.environ.get("PASSPORTSIM_OVERRIDE_CORPUS_ID")
    override_envs = (
        "PASSPORTSIM_OVERRIDE_REPO", "PASSPORTSIM_OVERRIDE_RUN_ID", "PASSPORTSIM_OVERRIDE_ARTIFACT",
        "PASSPORTSIM_OVERRIDE_BIN_SHA256", "PASSPORTSIM_OVERRIDE_ELF_SHA256",
    )
    if any(os.environ.get(env) for env in override_envs) and not override_id:
        raise RuntimeError("source override fields were supplied without PASSPORTSIM_OVERRIDE_CORPUS_ID")
    if override_id == args.corpus_id:
        override_names = {
            "repo": "PASSPORTSIM_OVERRIDE_REPO", "run_id": "PASSPORTSIM_OVERRIDE_RUN_ID",
            "artifact": "PASSPORTSIM_OVERRIDE_ARTIFACT",
        }
        missing_override = [env for env in override_names.values() if not os.environ.get(env)]
        if missing_override:
            raise RuntimeError("incomplete source override; missing " + ", ".join(missing_override))
        entry = dict(entry)
        for key, env in override_names.items():
            entry[key] = os.environ[env]
        entry["kind"] = "run"
        entry.pop("tag", None)
        entry.pop("asset", None)
        run_info = json.loads(run_gh(["api", f"repos/{entry['repo']}/actions/runs/{entry['run_id']}"]))
        if not run_info.get("head_sha"):
            raise RuntimeError("source override run metadata did not include head_sha")
        entry["source_commit"] = run_info["head_sha"]
        bin_override = os.environ.get("PASSPORTSIM_OVERRIDE_BIN_SHA256")
        elf_override = os.environ.get("PASSPORTSIM_OVERRIDE_ELF_SHA256")
        if not bin_override or not re.fullmatch(r"[0-9a-f]{64}", bin_override):
            raise RuntimeError("source override requires PASSPORTSIM_OVERRIDE_BIN_SHA256 as 64 lowercase hex characters")
        entry["bin_sha256"] = bin_override
        if entry.get("elf_member"):
            if not elf_override or not re.fullmatch(r"[0-9a-f]{64}", elf_override):
                raise RuntimeError("this corpus requires PASSPORTSIM_OVERRIDE_ELF_SHA256 as 64 lowercase hex characters")
            entry["elf_sha256"] = elf_override
    store = (args.store or Path(os.environ.get("PASSPORTSIM_FIRMWARE_STORE", "~/.cache/apsim/firmware"))).expanduser().resolve()
    store.mkdir(parents=True, exist_ok=True, mode=0o700)
    store.chmod(0o700)
    files_dir = store / "files" / args.corpus_id
    files_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    files_dir.chmod(0o700)
    names = {"bin": str(entry["bin_member"])}
    if entry.get("elf_member"):
        names["elf"] = str(entry["elf_member"])
    expected = {"bin": str(entry["bin_sha256"])}
    if "elf" in names:
        expected["elf"] = str(entry["elf_sha256"])
    paths: dict[str, Path] = {}
    actual: dict[str, str] = {}
    missing = False
    if entry["kind"] == "local-cache":
        if args.update_lock:
            raise RuntimeError("--update-lock is not supported for a pinned local-cache source")
        seed_dir = Path(str(entry["local_seed_dir"])).expanduser() if entry.get("local_seed_dir") else None
        if seed_dir is not None and not seed_dir.is_absolute():
            seed_dir = (LOCK.parent / seed_dir).resolve()
        for key, filename in names.items():
            destination = files_dir / Path(filename).name
            if destination.is_file() and sha256(destination) == expected[key]:
                paths[key] = destination
                actual[key] = expected[key]
                continue
            env_path = os.environ.get(ENV_NAMES[args.corpus_id][key])
            candidates = ([Path(env_path).expanduser()] if env_path else [])
            if seed_dir:
                candidates.append(seed_dir / Path(filename).name)
            seed_path = None
            mismatches: list[tuple[Path, str]] = []
            for candidate in candidates:
                if not candidate.is_file():
                    continue
                digest = sha256(candidate)
                if digest == expected[key]:
                    seed_path = candidate
                    break
                mismatches.append((candidate, digest))
            if seed_path is None and mismatches:
                bad_path, bad_digest = mismatches[0]
                raise RuntimeError(
                    f"SHA-256 mismatch for {args.corpus_id}.{key}: expected {expected[key]}, got {bad_digest} "
                    f"from {bad_path}; no available local seed matched the pin"
                )
            if seed_path is None:
                seed_help = f" or populate local_seed_dir {seed_dir}" if seed_dir else " or configure local_seed_dir"
                raise RuntimeError(
                    f"{args.corpus_id} is a temporary local-cache pin; verified {key} firmware is missing. "
                    f"Install {Path(filename).name} under {files_dir}, or set "
                    f"{ENV_NAMES[args.corpus_id][key]} to a matching file{seed_help}" +
                    ". CI skips this corpus until xiaoxiaole publishes a workflow run or release."
                )
            shutil.copyfile(seed_path, destination)
            destination.chmod(0o600)
            installed_digest = sha256(destination)
            if installed_digest != expected[key]:
                destination.unlink(missing_ok=True)
                raise RuntimeError(f"SHA-256 mismatch after installing {args.corpus_id}.{key} into the private store")
            paths[key] = destination
            actual[key] = installed_digest
    else:
        for key, filename in names.items():
            env_path = os.environ.get(ENV_NAMES[args.corpus_id][key])
            if env_path:
                path = Path(env_path).expanduser().resolve()
                if not path.is_file():
                    raise RuntimeError(f"configured {key} file does not exist: {path}")
                paths[key] = path
                actual[key] = sha256(path)
                continue
            path = files_dir / Path(filename).name
            if not path.is_file() or (not args.update_lock and sha256(path) != expected[key]):
                missing = True
            else:
                paths[key] = path
                actual[key] = sha256(path)
    if missing:
        with tempfile.TemporaryDirectory(prefix=f"{args.corpus_id}-", dir=store) as tmp:
            scratch = Path(tmp)
            downloaded = download(entry, scratch)
            for key, filename in names.items():
                if key in paths:
                    continue
                source_path = member_file(downloaded, filename)
                destination = files_dir / Path(filename).name
                shutil.copyfile(source_path, destination)
                destination.chmod(0o600)
                digest = sha256(destination)
                if not args.update_lock and digest != expected[key]:
                    destination.unlink(missing_ok=True)
                    raise RuntimeError(
                        f"SHA-256 mismatch for {args.corpus_id}.{key}: expected {expected[key]}, got {digest}; "
                        "review the source before using --update-lock"
                    )
                paths[key] = destination
                actual[key] = digest
    if args.update_lock:
        updates = {f"{key}_sha256": value for key, value in actual.items() if key in names and not os.environ.get(ENV_NAMES[args.corpus_id][key])}
        if updates:
            update_lock(args.corpus_id, updates)
    elif any(actual[key] != expected[key] for key in names if not os.environ.get(ENV_NAMES[args.corpus_id][key])):
        raise RuntimeError("verified store does not match the pinned source hashes")
    provenance = {
        "repo": entry["repo"], "kind": entry["kind"], "tag": entry.get("tag"),
        "run_id": entry.get("run_id"), "artifact": entry.get("artifact"), "asset": entry.get("asset"),
        "source_commit": entry["source_commit"],
        "files": {key: {"path": str(paths[key]), "sha256": actual[key]} for key in names},
        "elf_unavailable_reason": entry.get("elf_unavailable_reason"),
    }
    if entry["kind"] == "local-cache":
        provenance["temporary"] = bool(entry.get("temporary", False))
        provenance["temporary_reason"] = entry.get("temporary_reason")
    configure(args.home, args.corpus_id, paths, provenance)
    print(json.dumps(provenance, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, KeyError, ValueError, zipfile.BadZipFile) as error:
        print(f"fetch-firmware: {error}", file=sys.stderr)
        raise SystemExit(1)
