#!/usr/bin/env python3
"""Compare all screenshot hashes from two fresh runs and write top-level summary.json."""

import json
from pathlib import Path
import sys


UI_REQUIRED = {"chessfd8fa19", "xxlaba64c1"}


def evaluate(summaries):
    hashes = [[item["sha256"] for item in result["screenshot_list"]] for result in summaries]
    same = hashes[0] == hashes[1] and len(hashes[0]) > 0
    corpus_id = summaries[0]["corpus_id"]
    boot_pass = all(result["boot_pass"] for result in summaries)
    navigation_pass = all(result["navigation_pass"] for result in summaries)
    determinism_pass = same
    ui_tree_pass = corpus_id not in UI_REQUIRED or all(result.get("ui_tree_ok", False) for result in summaries)
    result = {
        "corpus_id": corpus_id,
        "exit_code": 0 if boot_pass and navigation_pass and determinism_pass and ui_tree_pass else 1,
        "boot_pass": boot_pass,
        "navigation_checks": {name: all(run["navigation_checks"][name] for run in summaries) for name in summaries[0]["navigation_checks"]},
        "navigation_pass": navigation_pass,
        "determinism_pass": determinism_pass,
        "ui_tree_required": corpus_id in UI_REQUIRED,
        "ui_tree_pass": ui_tree_pass,
        "ui_tree_reasons": [run.get("ui_tree_reason") for run in summaries if not run.get("ui_tree_ok", False)],
        "passed": boot_pass and navigation_pass and determinism_pass and ui_tree_pass,
        "runs": summaries,
        "determinism": {"screenshot_hashes_match": same, "checked_screenshots": min(map(len, hashes))},
    }
    if corpus_id == "xxlaba64c1":
        result["text_quality"] = "failing (known font/tofu issue, separately tracked)"
    return result


if __name__ == "__main__":
    root = Path(sys.argv[1])
    summaries = [json.loads((root / name / "summary.json").read_text()) for name in ("run1", "run2")]
    result = evaluate(summaries)
    (root / "summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    raise SystemExit(result["exit_code"])
