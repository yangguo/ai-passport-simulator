#!/usr/bin/env python3
"""Resolve the example corpus paths and write the simulator config plus digest manifest."""

from __future__ import annotations

import hashlib
import json
import os
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
IDS = {
    "gov011": {"bin": "PASSPORTSIM_GOV011_BIN"},
    "chessfd8fa19": {
        "bin": "PASSPORTSIM_CHESSFD8FA19_BIN",
        "elf": "PASSPORTSIM_CHESSFD8FA19_ELF",
    },
    "xxlaba64c1": {
        "bin": "PASSPORTSIM_XXLABA64C1_BIN",
        "elf": "PASSPORTSIM_XXLABA64C1_ELF",
    },
}
FILES = {
    ("gov011", "bin"): "ai-passport-go-v0.1.1-release-merged.bin",
    ("chessfd8fa19", "bin"): "merged.bin",
    ("chessfd8fa19", "elf"): "ai-passport-chess.elf",
    ("xxlaba64c1", "bin"): "merged.bin",
    ("xxlaba64c1", "elf"): "ai-passport-xiaoxiaole.elf",
}


def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            sha.update(chunk)
    return sha.hexdigest()


def configure(home: Path, corpus_id: str | None = None,
              paths: dict[str, Path] | None = None,
              source: dict[str, object] | None = None) -> tuple[Path, Path]:
    home = home.expanduser().resolve()
    firmware_dir = os.environ.get("FIRMWARE_DIR")
    entries: dict[str, dict[str, dict[str, str]]] = {}
    selected = [corpus_id] if corpus_id else list(IDS)
    tables: list[str] = []
    for selected_id in selected:
        if selected_id not in IDS:
            raise ValueError(f"unknown corpus id: {selected_id}")
        entries[selected_id] = {}
        table = [f"[{selected_id}]"]
        for key, env_name in IDS[selected_id].items():
            raw = str(paths[key]) if paths and key in paths else os.environ.get(env_name)
            if not raw:
                if not firmware_dir:
                    raise ValueError(f"set FIRMWARE_DIR or {env_name}")
                filename = FILES[(selected_id, key)]
                raw = str(Path(firmware_dir).expanduser() / selected_id / filename)
            path = Path(raw).expanduser().resolve()
            if not path.is_file():
                raise FileNotFoundError(f"{selected_id}.{key} does not exist: {path}")
            entries[selected_id][key] = {"path": str(path), "sha256": digest(path)}
            table.append(f'{key} = {json.dumps(str(path))}')
        hashes = ", ".join(f'{key} = "{item["sha256"]}"' for key, item in entries[selected_id].items())
        table.append(f"sha256 = {{ {hashes} }}")
        tables.append("\n".join(table))
    config_dir = home / "config"
    config_dir.mkdir(parents=True, exist_ok=True)
    corpus_file = config_dir / "corpus.toml"
    corpus_file.write_text("\n\n".join(tables) + "\n", encoding="utf-8")
    manifest = {
        "schema_version": 1,
        "corpus": entries,
    }
    if source:
        manifest["sources"] = {selected[0]: source}
    manifest_file = home / "firmware-manifest.json"
    manifest_file.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return corpus_file, manifest_file


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("home", type=Path)
    parser.add_argument("--corpus-id", choices=IDS)
    args = parser.parse_args()
    corpus_file, manifest_file = configure(args.home, args.corpus_id)
    print(corpus_file)
    print(manifest_file)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"configure-corpus: {error}", file=sys.stderr)
        raise SystemExit(1)
