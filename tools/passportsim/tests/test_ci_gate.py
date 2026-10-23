"""Offline regression checks for firmware comparison and warning policy."""
from __future__ import annotations

import importlib.util
from pathlib import Path
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
