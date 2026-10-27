#!/usr/bin/env python3
"""Build a stable, compact summary from one batch execution."""

import argparse
import hashlib
import json
from pathlib import Path
import re


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


parser = argparse.ArgumentParser()
parser.add_argument("--run-dir", required=True, type=Path)
parser.add_argument("--manifest", required=True, type=Path)
parser.add_argument("--corpus-id", required=True)
parser.add_argument("--cli-exit-code", required=True, type=int)
args = parser.parse_args()
log = (args.run_dir / "session.log").read_text(encoding="utf-8", errors="replace")
manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
elf_sha = manifest["corpus"][args.corpus_id].get("elf", {}).get("sha256")
firmware_source = manifest.get("sources", {}).get(args.corpus_id)
screenshots = [
    {"path": str(path.relative_to(args.run_dir)), "sha256": sha256(path), "file": path}
    for path in sorted((args.run_dir / "artifacts").rglob("*.png"))
]
vt_values = re.findall(r'"vt_us"\s*:\s*(\d+)', log)
idf_matches = re.findall(r'(?:IDF_VERSION|ESP-IDF(?: version)?)\s*[:= ]\s*(v?\d+\.\d+\.\d+)', log, re.I)
command_pattern = re.compile(r"^>>> (\d+): (.*?)\n(.*?)^<<< exit (\d+)$", re.M | re.S)
commands = []
uart_output = []
for line, command, output, code_text in command_pattern.findall(log):
    code = int(code_text)
    kind = command.split(maxsplit=1)[0]
    if kind == "serial" and command.startswith("serial read"):
        uart_output.append(output.rstrip())
    known_ui_reason = None
    if kind == "ui" and code != 0:
        if args.corpus_id == "gov011" and "no symbol the app ELF of this firmware" in output:
            known_ui_reason = "Go firmware has no ELF supplied; ui reports E_STATE (no symbol)."
    tolerated = (kind == "run" and "--for" in command and code == 10) or (
        kind == "ui" and known_ui_reason is not None
    )
    commands.append({
        "line": int(line), "command": command, "exit_code": code,
        "accepted": code == 0 or tolerated,
        "accepted_as": (
            "pass_with_caveats (timed run completed)" if kind == "run" and code == 10 and "--for" in command
            else "known_limitation" if known_ui_reason
            else None
        ),
        "known_limitation_reason": known_ui_reason,
    })
ui_commands = [item for item in commands if item["command"].startswith("ui ")]
ui_reasons = list(dict.fromkeys(
    item["known_limitation_reason"] for item in ui_commands if item["known_limitation_reason"]
))
ui_tree_ok = bool(ui_commands) and all(item["exit_code"] == 0 for item in ui_commands)
ui_tree_reason = None if ui_tree_ok else "; ".join(ui_reasons) if ui_reasons else "One or more ui commands failed; see session.log."
by_name = {Path(item["file"]).name: item for item in screenshots}
navigation_names = ["01-boot.png", "02-after-down.png", "03-after-up.png", "04-after-ok.png"]
navigation = {
    "boot_screenshot_nonblank": False,
    "after_down_differs_from_boot": False,
    "after_up_differs_from_previous": False,
    "after_ok_differs_from_previous": False,
}
for name in navigation_names:
    item = by_name.get(name)
    if item is None:
        continue
    try:
        from PIL import Image, ImageChops
        image = Image.open(item["file"]).convert("RGB")
        if name == "01-boot.png":
            navigation["boot_screenshot_nonblank"] = ImageChops.difference(image, Image.new("RGB", image.size, (0, 0, 0))).getbbox() is not None
    except Exception:
        pass
for field, previous_name, current_name in [
    ("after_down_differs_from_boot", "01-boot.png", "02-after-down.png"),
    ("after_up_differs_from_previous", "02-after-down.png", "03-after-up.png"),
    ("after_ok_differs_from_previous", "03-after-up.png", "04-after-ok.png"),
]:
    if previous_name in by_name and current_name in by_name:
        navigation[field] = by_name[previous_name]["sha256"] != by_name[current_name]["sha256"]
navigation_pass = all(navigation.values())
text_quality = (
    "unassessed (temporary font-fix pin)"
    if args.corpus_id == "xxlaba64c1" else None
)
command_failures = [item for item in commands if not item["accepted"]]
summary = {
    "corpus_id": args.corpus_id,
    "cli_exit_code": args.cli_exit_code,
    "exit_code": 0 if not command_failures else 1,
    "commands": [{k: v for k, v in item.items() if k != "file"} for item in commands],
    "command_failures": command_failures,
    "boot_pass": navigation["boot_screenshot_nonblank"] and any(item["command"].startswith("start ") and item["exit_code"] == 0 for item in commands),
    "navigation_checks": navigation,
    "navigation_pass": navigation_pass,
    "determinism_pass": None,
    "ui_tree_ok": ui_tree_ok,
    "ui_tree_reason": ui_tree_reason,
    "ui_command_exit_codes": [item["exit_code"] for item in ui_commands],
    "text_quality": text_quality,
    "screenshot_list": [{k: v for k, v in item.items() if k != "file"} for item in screenshots],
    "vt_us": int(vt_values[-1]) if vt_values else None,
    "idf_ver": idf_matches[-1] if idf_matches else None,
    "elf_sha256": elf_sha,
    "firmware_source": firmware_source,
}
(args.run_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
(args.run_dir / "uart.log").write_text("\n".join(uart_output).rstrip() + "\n", encoding="utf-8")
