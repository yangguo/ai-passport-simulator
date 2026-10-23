#!/usr/bin/env python3
"""Check fixture glyph inventory against the generated LVGL font's glyph comments."""

import json
from pathlib import Path
import re
import sys

from gen_host_font import EMOJI, OUT, SCENARIOS, strings


def main() -> int:
    fixture_codepoints = {
        ord(char)
        for path in sorted(SCENARIOS.glob("*.json"))
        if not path.name.startswith("_bad_")
        for value in strings(json.loads(path.read_text(encoding="utf-8")))
        for char in value
        if char not in EMOJI
    }
    fixture_codepoints.update(range(0x20, 0x7F))
    if not OUT.is_file():
        print(f"missing generated host font: {OUT}", file=sys.stderr)
        return 1
    generated = OUT.read_text(encoding="utf-8")
    covered = {int(value, 16) for value in re.findall(r"/\* U\+([0-9A-Fa-f]{4,6})", generated)}
    missing = sorted(fixture_codepoints - covered)
    if missing:
        print("host font is missing: " + ", ".join(f"U+{cp:04X}" for cp in missing), file=sys.stderr)
        return 1
    print(f"host font covers {len(fixture_codepoints)} required fixture codepoints; emoji use placeholders")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
