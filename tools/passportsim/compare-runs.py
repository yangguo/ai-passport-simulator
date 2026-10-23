#!/usr/bin/env python3
"""Compare all screenshot hashes from two fresh runs and write top-level summary.json."""

import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
summaries = [json.loads((root / name / "summary.json").read_text()) for name in ("run1", "run2")]
hashes = [[item["sha256"] for item in result["screenshot_list"]] for result in summaries]
same = hashes[0] == hashes[1] and len(hashes[0]) > 0
boot_pass = all(result["boot_pass"] for result in summaries)
navigation_pass = all(result["navigation_pass"] for result in summaries)
determinism_pass = same
exit_code = 0 if boot_pass and navigation_pass and determinism_pass else 1
result = {
    "corpus_id": summaries[0]["corpus_id"],
    "exit_code": exit_code,
    "boot_pass": boot_pass,
    "navigation_checks": {name: all(run["navigation_checks"][name] for run in summaries) for name in summaries[0]["navigation_checks"]},
    "navigation_pass": navigation_pass,
    "determinism_pass": determinism_pass,
    "runs": summaries,
    "determinism": {"screenshot_hashes_match": same, "checked_screenshots": min(map(len, hashes))},
}
if summaries[0]["corpus_id"] == "xxlaba64c1":
    result["text_quality"] = "failing (known font/tofu issue, separately tracked)"
(root / "summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
raise SystemExit(exit_code)
