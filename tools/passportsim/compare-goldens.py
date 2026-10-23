#!/usr/bin/env python3
"""Compare firmware checkpoint screenshots with reviewed, firmware-bound goldens."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
GOLDENS = ROOT / "tests" / "firmware-goldens"
CHECKPOINTS = ("01-boot", "02-after-down", "03-after-up", "04-after-ok")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def find_checkpoint(run_dir: Path, checkpoint: str) -> Path:
    matches = list((run_dir / "run1" / "artifacts").rglob(f"{checkpoint}.png"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one {checkpoint}.png under run1 artifacts; found {len(matches)}")
    return matches[0]


def compare_checkpoint(actual_path: Path, expected_path: Path | None,
                       metadata: dict[str, object], output_dir: Path) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)
    name = actual_path.stem
    actual_copy = output_dir / f"{name}.actual.png"
    shutil.copyfile(actual_path, actual_copy)
    with Image.open(actual_path) as actual_image:
        actual = actual_image.convert("RGB")
    width, height = actual.size
    actual_hash = sha256_file(actual_path)
    expected_hash = metadata.get("sha256")
    dimensions_match = (width, height) == (metadata.get("width"), metadata.get("height"))
    pixel_count = width * height
    changed_pixels: int | None = None
    diff_path: Path | None = None
    expected_copy: Path | None = None
    expected_image_valid = True
    if expected_path and expected_path.is_file():
        expected_image_valid = sha256_file(expected_path) == expected_hash
        with Image.open(expected_path) as expected_image:
            expected = expected_image.convert("RGB")
        expected_copy = output_dir / f"{name}.expected.png"
        shutil.copyfile(expected_path, expected_copy)
        if expected.size == actual.size:
            diff = ImageChops.difference(actual, expected)
            pixels = diff.tobytes()
            changed_pixels = sum(1 for offset in range(0, len(pixels), 3)
                                 if pixels[offset] or pixels[offset + 1] or pixels[offset + 2])
            diff_path = output_dir / f"{name}.diff.png"
            diff.save(diff_path)
        else:
            changed_pixels = pixel_count
            diff_path = output_dir / f"{name}.diff.png"
            Image.new("RGB", actual.size, (255, 0, 255)).save(diff_path)
    passed = actual_hash == expected_hash and dimensions_match and expected_image_valid
    return {
        "checkpoint": name, "passed": passed, "actual_sha256": actual_hash,
        "expected_sha256": expected_hash, "width": width, "height": height,
        "dimensions_match": dimensions_match, "changed_pixels": changed_pixels,
        "expected_image_valid": expected_image_valid,
        "actual_png": str(actual_copy), "expected_png": str(expected_copy) if expected_copy else None,
        "diff_png": str(diff_path) if diff_path else None,
        "text_quality": metadata.get("text_quality", "unassessed"),
    }


def run(run_dir: Path, corpus_id: str, update: bool = False) -> dict[str, object]:
    golden_dir = GOLDENS / corpus_id
    golden_path = golden_dir / "golden.json"
    manifest_path = run_dir / "run1" / "home" / "firmware-manifest.json"
    if not manifest_path.is_file():
        manifest_path = run_dir / "home" / "firmware-manifest.json"
    if not manifest_path.is_file():
        raise RuntimeError(f"firmware manifest missing from {run_dir}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    firmware = manifest.get("corpus", {}).get(corpus_id, {}).get("bin", {}).get("sha256")
    if not firmware:
        firmware = manifest.get("sources", {}).get(corpus_id, {}).get("files", {}).get("bin", {}).get("sha256")
    if not firmware:
        raise RuntimeError(f"could not determine firmware SHA-256 for {corpus_id}")
    existing: dict[str, object] = {}
    if golden_path.is_file():
        existing = json.loads(golden_path.read_text(encoding="utf-8"))
        if existing.get("firmware_sha256") != firmware and not update:
            raise RuntimeError(
                f"golden invalid for {corpus_id}: belongs to firmware {existing.get('firmware_sha256')}, "
                f"run uses {firmware}; review/update the golden explicitly"
            )
        if existing.get("firmware_sha256") != firmware and update:
            print(f"golden firmware identity changes: {existing.get('firmware_sha256')} -> {firmware}", file=sys.stderr)

    checkpoints: dict[str, object] = {}
    results = []
    for checkpoint in CHECKPOINTS:
        filename = f"{checkpoint}.png"
        actual_path = find_checkpoint(run_dir, checkpoint)
        with Image.open(actual_path) as image:
            width, height = image.size
        old = existing.get("checkpoints", {}).get(checkpoint, {})
        text_quality = old.get("text_quality") if isinstance(old, dict) else None
        if corpus_id == "xxlaba64c1":
            text_quality = "failing-known-tofu"
        if update:
            if text_quality == "failing-known-tofu":
                print(f"{corpus_id}/{checkpoint}: recording navigation baseline; text quality remains failing-known-tofu")
            checkpoints[checkpoint] = {
                "sha256": sha256_file(actual_path), "width": width, "height": height,
                **({"text_quality": text_quality} if text_quality else {}),
            }
            continue
        metadata = existing.get("checkpoints", {}).get(checkpoint)
        if not isinstance(metadata, dict):
            raise RuntimeError(f"golden checkpoint metadata missing: {golden_path}::{checkpoint}")
        expected_path = golden_dir / filename
        result = compare_checkpoint(actual_path, expected_path, metadata, run_dir / "comparison")
        results.append(result)
        print(f"{corpus_id}/{checkpoint}: {'PASS' if result['passed'] else 'FAIL'} sha256={result['actual_sha256']} changed_pixels={result['changed_pixels']}")
    if update:
        golden_dir.mkdir(parents=True, exist_ok=True)
        golden_path.write_text(json.dumps({
            "corpus_id": corpus_id, "firmware_sha256": firmware, "checkpoints": checkpoints,
        }, indent=2) + "\n", encoding="utf-8")
        # Chess is public; the two private-game PNGs stay outside the repository.
        if corpus_id == "chessfd8fa19":
            for checkpoint in CHECKPOINTS:
                shutil.copyfile(find_checkpoint(run_dir, checkpoint), golden_dir / f"{checkpoint}.png")
        else:
            for checkpoint in CHECKPOINTS:
                (golden_dir / f"{checkpoint}.png").unlink(missing_ok=True)
        return {"passed": True, "updated": True, "firmware_sha256": firmware, "checkpoints": checkpoints}
    text_failed = corpus_id == "xxlaba64c1"
    result = {"passed": all(item["passed"] for item in results), "updated": False,
              "firmware_sha256": firmware, "checkpoints": results,
              "text_quality_pass": not text_failed,
              "text_quality": "failing-known-tofu" if text_failed else "unassessed"}
    if text_failed:
        print("TEXT QUALITY: FAIL (known tofu checkpoints; navigation pixels remain gated)", file=sys.stderr)
    (run_dir / "golden-results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus_id", choices=("gov011", "chessfd8fa19", "xxlaba64c1"))
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--update", action="store_true")
    args = parser.parse_args()
    try:
        result = run(args.run_dir.resolve(), args.corpus_id, args.update)
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f"compare-goldens: {error}", file=sys.stderr)
        return 1
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
