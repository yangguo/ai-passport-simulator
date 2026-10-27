"""Offline regression checks for firmware comparison and warning policy."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

class CiGateRegressionTests(unittest.TestCase):
    def test_missing_temporary_local_cache_is_an_explicit_nonblocking_skip(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            env = dict(__import__("os").environ)
            env["PASSPORTSIM_FIRMWARE_STORE"] = str(root / "empty-store")
            result = subprocess.run(
                [str(ROOT / "ci-gate.sh"), "--corpora", "xxlaba64c1", "--runs-root", str(root / "runs")],
                cwd=ROOT.parents[1], env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn("CI skips xxlaba64c1", result.stdout)
            summary = json.loads((root / "runs" / "summary.json").read_text())
            self.assertTrue(summary["passed"])
            self.assertTrue(summary["corpora"][0]["skipped"])
            self.assertIn("until xiaoxiaole publishes", summary["corpora"][0]["skip_reason"])

    def test_lvgl_flags_error_is_no_longer_a_chess_limitation(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "artifacts").mkdir()
            (root / "session.log").write_text(
                ">>> 1: ui --max-nodes 300\nstruct _lv_obj_t has no member flags\n<<< exit 1\n",
                encoding="utf-8",
            )
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({"corpus": {"chessfd8fa19": {"elf": {"sha256": "abc"}}}}))
            subprocess.run(
                [sys.executable, str(ROOT / "summarize-run.py"), "--run-dir", str(root),
                 "--manifest", str(manifest), "--corpus-id", "chessfd8fa19", "--cli-exit-code", "1"],
                check=True,
            )
            summary = json.loads((root / "summary.json").read_text())
            self.assertFalse(summary["commands"][0]["accepted"])
            self.assertIsNone(summary["commands"][0]["known_limitation_reason"])

    def test_missing_go_elf_remains_a_known_ui_skip(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "artifacts").mkdir()
            (root / "session.log").write_text(
                ">>> 1: ui --max-nodes 300\nno symbol the app ELF of this firmware\n<<< exit 1\n",
                encoding="utf-8",
            )
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({"corpus": {"gov011": {}}}))
            subprocess.run(
                [sys.executable, str(ROOT / "summarize-run.py"), "--run-dir", str(root),
                 "--manifest", str(manifest), "--corpus-id", "gov011", "--cli-exit-code", "1"],
                check=True,
            )
            summary = json.loads((root / "summary.json").read_text())
            self.assertTrue(summary["commands"][0]["accepted"])
            self.assertEqual(summary["commands"][0]["accepted_as"], "known_limitation")

    def test_xxl_run_summary_leaves_text_quality_to_golden_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "artifacts").mkdir()
            (root / "session.log").write_text("", encoding="utf-8")
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({"corpus": {"xxlaba64c1": {"elf": {"sha256": "abc"}}}}))
            subprocess.run(
                [sys.executable, str(ROOT / "summarize-run.py"), "--run-dir", str(root),
                 "--manifest", str(manifest), "--corpus-id", "xxlaba64c1", "--cli-exit-code", "0"],
                check=True,
            )
            summary = json.loads((root / "summary.json").read_text())
            self.assertIsNone(summary["text_quality"])

    def test_ui_tree_is_required_for_chess_and_xxl_but_skipped_for_go(self) -> None:
        compare = load("compare_runs", "compare-runs.py")
        base = {
            "boot_pass": True,
            "navigation_pass": True,
            "navigation_checks": {"boot_screenshot_nonblank": True},
            "screenshot_list": [{"sha256": "same"}],
            "ui_tree_ok": False,
            "ui_tree_reason": "missing ELF",
        }
        for corpus_id in ("chessfd8fa19", "xxlaba64c1"):
            result = compare.evaluate([dict(base, corpus_id=corpus_id), dict(base, corpus_id=corpus_id)])
            self.assertFalse(result["ui_tree_pass"], corpus_id)
            self.assertFalse(result["passed"], corpus_id)
        go = compare.evaluate([dict(base, corpus_id="gov011"), dict(base, corpus_id="gov011")])
        self.assertTrue(go["ui_tree_pass"])
        self.assertTrue(go["passed"])

    def test_ui_tree_success_allows_chess_and_xxl_gate(self) -> None:
        compare = load("compare_runs", "compare-runs.py")
        for corpus_id in ("chessfd8fa19", "xxlaba64c1"):
            summary = {
                "corpus_id": corpus_id, "boot_pass": True,
                "navigation_pass": True,
                "navigation_checks": {"boot_screenshot_nonblank": True},
                "screenshot_list": [{"sha256": "same"}], "ui_tree_ok": True,
            }
            result = compare.evaluate([summary, summary])
            self.assertTrue(result["ui_tree_pass"])
            self.assertTrue(result["passed"])
            self.assertNotIn("text_quality", result)

    def test_modified_screenshot_fails_golden_comparator(self) -> None:
        compare = load("compare_goldens", "compare-goldens.py")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            run = root / "run1" / "artifacts"
            run.mkdir(parents=True)
            golden_dir = root / "golden"
            golden_dir.mkdir()
            expected = Image.new("RGB", (3, 2), (0, 0, 0))
            actual = expected.copy()
            actual.putpixel((1, 0), (255, 0, 0))
            expected.save(golden_dir / "01-boot.png")
            actual.save(run / "01-boot.png")
            result = compare.compare_checkpoint(
                run / "01-boot.png", golden_dir / "01-boot.png",
                {"sha256": compare.sha256_file(golden_dir / "01-boot.png"), "width": 3, "height": 2},
                root / "out",
            )
            self.assertFalse(result["passed"])
            self.assertEqual(result["changed_pixels"], 1)
            self.assertTrue((root / "out" / "01-boot.actual.png").is_file())
            self.assertTrue((root / "out" / "01-boot.diff.png").is_file())

    def test_new_xxl_firmware_golden_is_text_unassessed_and_keeps_png_private(self) -> None:
        compare = load("compare_goldens", "compare-goldens.py")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            compare.GOLDENS = root / "goldens"
            run = root
            artifacts = run / "run1" / "artifacts"
            artifacts.mkdir(parents=True)
            for checkpoint in compare.CHECKPOINTS:
                Image.new("RGB", (3, 2), (1, 2, 3)).save(artifacts / f"{checkpoint}.png")
            home = run / "run1" / "home"
            home.mkdir()
            (home / "firmware-manifest.json").write_text(json.dumps({
                "corpus": {"xxlaba64c1": {"bin": {"sha256": "font-fix-hash"}}}
            }))
            result = compare.run(run, "xxlaba64c1", update=True)
            self.assertEqual(result["text_quality"], "unassessed")
            self.assertIsNone(result["text_quality_pass"])
            metadata = json.loads((compare.GOLDENS / "xxlaba64c1" / "golden.json").read_text())
            self.assertTrue(all(item["text_quality"] == "unassessed" for item in metadata["checkpoints"].values()))
            self.assertFalse(any((compare.GOLDENS / "xxlaba64c1").glob("*.png")))

    def test_unallowlisted_warning_fails_warning_gate(self) -> None:
        warnings = load("check_warnings", "check-warnings.py")
        signatures = warnings.extract_warnings("INFO ready\nWARN new register read at 0x6000\n")
        result = warnings.evaluate(signatures, {"WARN reviewed warning"})
        self.assertFalse(result["passed"])
        self.assertEqual(result["new"], ["WARN new register read at 0x6000"])

    def test_pass_with_caveats_is_a_warning_signature(self) -> None:
        warnings = load("check_warnings", "check-warnings.py")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            run = root / "run1"
            run.mkdir()
            (run / "summary.json").write_text(
                '{"commands":[{"accepted_as":"pass_with_caveats (timed run completed)"}]}',
                encoding="utf-8",
            )
            observed = warnings.extract_from_run(root)
            self.assertEqual(observed, ["pass_with_caveats: pass_with_caveats (timed run completed)"])

if __name__ == "__main__":
    unittest.main()
