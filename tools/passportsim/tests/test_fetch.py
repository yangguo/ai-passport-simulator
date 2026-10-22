#!/usr/bin/env python3
"""Offline fetcher smoke test using a fake gh executable."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import tempfile
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "fetch-firmware.py"
spec = importlib.util.spec_from_file_location("fetch_firmware", SCRIPT)
assert spec and spec.loader
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="apsim-fetch-test-") as temporary:
        root = Path(temporary)
        fake_bin = root / "bin"
        fake_bin.mkdir()
        fixtures = root / "fixtures"
        fixtures.mkdir()
        release_payload = b"offline release firmware payload"
        run_bin = b"offline merged firmware"
        run_elf = b"offline debug symbols"
        release_zip = fixtures / "release.zip"
        with zipfile.ZipFile(release_zip, "w") as archive:
            archive.writestr("nested/ai-passport-go-v0.1.1-release-merged.bin", release_payload)
        artifact_dir = fixtures / "artifact"
        artifact_dir.mkdir()
        (artifact_dir / "merged.bin").write_bytes(run_bin)
        (artifact_dir / "ai-passport-chess.elf").write_bytes(run_elf)
        calls = root / "gh-calls.txt"
        fake_gh = fake_bin / "gh"
        fake_gh.write_text(f'''#!/usr/bin/env python3
import json, os, shutil, sys
from pathlib import Path
args = sys.argv[1:]
with open(os.environ["FAKE_GH_CALLS"], "a") as stream:
    stream.write(" ".join(args) + "\\n")
if args[:2] == ["release", "download"]:
    dest = Path(args[args.index("-D") + 1]); dest.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(os.environ["FAKE_RELEASE_ZIP"], dest / args[args.index("-p") + 1])
elif args[0] == "api":
    print(json.dumps({{"artifacts": [{{"name": "fixture-artifact", "expired": False}}]}}))
elif args[:2] == ["run", "download"]:
    dest = Path(args[args.index("-D") + 1]); dest.mkdir(parents=True, exist_ok=True)
    for item in Path(os.environ["FAKE_ARTIFACT_DIR"]).iterdir():
        shutil.copyfile(item, dest / item.name)
else:
    raise SystemExit("unexpected gh invocation: " + repr(args))
''', encoding="utf-8")
        fake_gh.chmod(fake_gh.stat().st_mode | stat.S_IXUSR)
        os.environ.update({
            "PATH": f"{fake_bin}{os.pathsep}{os.environ['PATH']}",
            "FAKE_GH_CALLS": str(calls), "FAKE_RELEASE_ZIP": str(release_zip),
            "FAKE_ARTIFACT_DIR": str(artifact_dir),
        })
        lock = root / "firmware-sources.toml"
        lock.write_text(f'''[corpora.gov011]
repo = "fixture/go"
kind = "release"
tag = "v-test"
asset = "fixture.zip"
source_commit = "commit-go"
bin_member = "ai-passport-go-v0.1.1-release-merged.bin"
bin_sha256 = "{digest(release_payload)}"
elf_member = ""
elf_sha256 = ""
elf_unavailable_reason = "no elf"

[corpora.chessfd8fa19]
repo = "fixture/chess"
kind = "run"
run_id = 123
artifact = "fixture-artifact"
source_commit = "commit-chess"
bin_member = "merged.bin"
bin_sha256 = "{digest(run_bin)}"
elf_member = "ai-passport-chess.elf"
elf_sha256 = "{digest(run_elf)}"
''', encoding="utf-8")
        fetch.LOCK = lock
        store = root / "private-store"
        for corpus_id in ("gov011", "chessfd8fa19"):
            home = root / f"home-{corpus_id}"
            fetch.sys.argv = [str(SCRIPT), corpus_id, "--home", str(home), "--store", str(store)]
            assert fetch.main() == 0
            manifest = json.loads((home / "firmware-manifest.json").read_text())
            assert manifest["sources"][corpus_id]["source_commit"].startswith("commit-")
            assert (home / "config" / "corpus.toml").is_file()
        before = calls.read_text().splitlines()
        fetch.sys.argv = [str(SCRIPT), "gov011", "--home", str(root / "home-repeat"), "--store", str(store)]
        assert fetch.main() == 0
        assert calls.read_text().splitlines() == before, "verified files should avoid another gh call"
        assert stat.S_IMODE(store.stat().st_mode) == 0o700
        print("test_fetch: release, run artifact, manifest, private permissions, and cache reuse passed")


if __name__ == "__main__":
    main()
