#!/usr/bin/env python3
"""Resolve the example corpus paths and write the simulator config plus digest manifest."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
IDS = {
    "gov011": {"bin": "PASSPORTSIM_GOV011_BIN"},
    "chess93dd170": {
        "bin": "PASSPORTSIM_CHESS93DD170_BIN",
        "elf": "PASSPORTSIM_CHESS93DD170_ELF",
    },
    "xxlcac7eb1": {
        "bin": "PASSPORTSIM_XXLCAC7EB1_BIN",
        "elf": "PASSPORTSIM_XXLCAC7EB1_ELF",
    },
}
FILES = {
    ("gov011", "bin"): "ai-passport-go-v0.1.1-release-merged.bin",
    ("chess93dd170", "bin"): "merged.bin",
    ("chess93dd170", "elf"): "ai-passport-chess.elf",
    ("xxlcac7eb1", "bin"): "merged.bin",
    ("xxlcac7eb1", "elf"): "ai-passport-xiaoxiaole.elf",
}


def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            sha.update(chunk)
    return sha.hexdigest()


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: configure-corpus.py <PASSPORTSIM_HOME>", file=sys.stderr)
        return 2
    home = Path(sys.argv[1]).expanduser().resolve()
    firmware_dir = os.environ.get("FIRMWARE_DIR")
    template = (ROOT / "corpus.example.toml").read_text(encoding="utf-8")
    entries: dict[str, dict[str, dict[str, str]]] = {}
    for corpus_id, keys in IDS.items():
        entries[corpus_id] = {}
        for key, env_name in keys.items():
            raw = os.environ.get(env_name)
            if not raw:
                if not firmware_dir:
                    raise ValueError(f"set FIRMWARE_DIR or {env_name}")
                filename = FILES[(corpus_id, key)]
                raw = str(Path(firmware_dir).expanduser() / corpus_id / filename)
            path = Path(raw).expanduser().resolve()
            if not path.is_file():
                raise FileNotFoundError(f"{corpus_id}.{key} does not exist: {path}")
            entries[corpus_id][key] = {"path": str(path), "sha256": digest(path)}
            placeholder_path = f"${{FIRMWARE_DIR}}/{corpus_id}/{FILES[(corpus_id, key)]}"
            quoted_placeholder = json.dumps(placeholder_path)
            if quoted_placeholder not in template:
                raise ValueError(f"missing template path for {corpus_id}.{key}")
            template = template.replace(quoted_placeholder, json.dumps(str(path)), 1)

    # Pin each resolved payload in the existing TOML table so a changed file fails closed.
    lines = []
    for line in template.rstrip().splitlines():
        lines.append(line)
        if line.startswith("[") and line.endswith("]"):
            corpus_id = line[1:-1]
            files = entries[corpus_id]
            hashes = ", ".join(f'{key} = "{item["sha256"]}"' for key, item in files.items())
            lines.append(f"sha256 = {{ {hashes} }}")
    template = "\n".join(lines) + "\n"
    config_dir = home / "config"
    config_dir.mkdir(parents=True, exist_ok=True)
    (config_dir / "corpus.toml").write_text(template, encoding="utf-8")
    manifest = {
        "schema_version": 1,
        "corpus": entries,
    }
    (home / "firmware-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(home / "config" / "corpus.toml")
    print(home / "firmware-manifest.json")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"configure-corpus: {error}", file=sys.stderr)
        raise SystemExit(1)
