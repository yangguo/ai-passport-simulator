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
elif args[0] == "api" and len(args) > 1 and args[1].startswith("repos/") and "/actions/runs/" in args[1] and not args[1].endswith("/artifacts"):
    print(json.dumps({{"head_sha": "override-source-commit"}}))
elif args[0] == "api":
    print(json.dumps({{"artifacts": [{{"name": "fixture-artifact", "expired": False}}, {{"name": "fixture-artifact-override", "expired": False}}]}}))
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

[corpora.xxlaba64c1]
repo = "fixture/xxl"
kind = "local-cache"
temporary = true
temporary_reason = "awaiting published run"
source_commit = "57a23fc5e093696cc0c226c1d265969a3fadc4e2"
bin_member = "merged.bin"
bin_sha256 = "{digest(run_bin)}"
elf_member = "ai-passport-xiaoxiaole.elf"
elf_sha256 = "{digest(run_elf)}"
local_seed_dir = "fixture-seed"
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
        seed_dir = root / "fixture-seed"
        seed_dir.mkdir()
        (seed_dir / "merged.bin").write_bytes(run_bin)
        (seed_dir / "ai-passport-xiaoxiaole.elf").write_bytes(run_elf)
        before_local = calls.read_text().splitlines()
        local_home = root / "home-local-cache"
        fetch.sys.argv = [str(SCRIPT), "xxlaba64c1", "--home", str(local_home), "--store", str(store)]
        assert fetch.main() == 0
        local_source = json.loads((local_home / "firmware-manifest.json").read_text())["sources"]["xxlaba64c1"]
        assert local_source["kind"] == "local-cache" and local_source["temporary"] is True
        assert local_source["source_commit"] == "57a23fc5e093696cc0c226c1d265969a3fadc4e2"
        assert local_source["files"]["bin"]["sha256"] == digest(run_bin)
        assert stat.S_IMODE((store / "files/xxlaba64c1/merged.bin").stat().st_mode) == 0o600
        assert calls.read_text().splitlines() == before_local, "local-cache must never invoke gh"
        bad_env_bin = root / "bad-env-merged.bin"
        bad_env_bin.write_bytes(b"wrong firmware from the first seed source")
        os.environ["PASSPORTSIM_XXLABA64C1_BIN"] = str(bad_env_bin)
        os.environ["PASSPORTSIM_XXLABA64C1_ELF"] = str(seed_dir / "ai-passport-xiaoxiaole.elf")
        env_store = root / "env-store"
        env_home = root / "home-env-seed"
        fetch.sys.argv = [str(SCRIPT), "xxlaba64c1", "--home", str(env_home), "--store", str(env_store)]
        assert fetch.main() == 0
        assert (env_store / "files/xxlaba64c1/merged.bin").is_file()
        assert digest((env_store / "files/xxlaba64c1/merged.bin").read_bytes()) == digest(run_bin)
        assert stat.S_IMODE((env_store / "files/xxlaba64c1/merged.bin").stat().st_mode) == 0o600
        del os.environ["PASSPORTSIM_XXLABA64C1_BIN"]
        del os.environ["PASSPORTSIM_XXLABA64C1_ELF"]
        import shutil
        shutil.rmtree(seed_dir)
        empty_store = root / "empty-store"
        fetch.sys.argv = [str(SCRIPT), "xxlaba64c1", "--home", str(root / "home-missing"), "--store", str(empty_store)]
        try:
            fetch.main()
        except RuntimeError as error:
            assert "temporary local-cache pin" in str(error)
            assert "CI skips this corpus" in str(error)
        else:
            raise AssertionError("missing local-cache firmware should fail with install guidance")
        before = calls.read_text().splitlines()
        fetch.sys.argv = [str(SCRIPT), "gov011", "--home", str(root / "home-repeat"), "--store", str(store)]
        assert fetch.main() == 0
        assert calls.read_text().splitlines() == before, "verified files should avoid another gh call"
        os.environ.update({
            "PASSPORTSIM_OVERRIDE_CORPUS_ID": "chessfd8fa19",
            "PASSPORTSIM_OVERRIDE_REPO": "fixture/chess-override",
            "PASSPORTSIM_OVERRIDE_RUN_ID": "456",
            "PASSPORTSIM_OVERRIDE_ARTIFACT": "fixture-artifact-override",
            "PASSPORTSIM_OVERRIDE_BIN_SHA256": digest(run_bin),
            "PASSPORTSIM_OVERRIDE_ELF_SHA256": digest(run_elf),
        })
        unrelated_home = root / "home-unrelated-while-overridden"
        fetch.sys.argv = [str(SCRIPT), "gov011", "--home", str(unrelated_home), "--store", str(store)]
        assert fetch.main() == 0
        unrelated_source = json.loads((unrelated_home / "firmware-manifest.json").read_text())["sources"]["gov011"]
        assert unrelated_source["repo"] == "fixture/go", "single-game override must not affect other corpus entries"
        override_home = root / "home-override"
        fetch.sys.argv = [str(SCRIPT), "chessfd8fa19", "--home", str(override_home), "--store", str(store)]
        assert fetch.main() == 0
        override_source = json.loads((override_home / "firmware-manifest.json").read_text())["sources"]["chessfd8fa19"]
        assert override_source["repo"] == "fixture/chess-override"
        assert override_source["run_id"] == "456"
        assert override_source["artifact"] == "fixture-artifact-override"
        assert override_source["source_commit"] == "override-source-commit"
        assert stat.S_IMODE(store.stat().st_mode) == 0o700
        print("test_fetch: release, run artifact, manifest, private permissions, and cache reuse passed")


if __name__ == "__main__":
    main()
