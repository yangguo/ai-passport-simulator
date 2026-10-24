#!/usr/bin/env python3
"""Generate the host LVGL shell's Noto Sans SC subset (14 px, 4 bpp).

The subset covers ASCII plus visible text in valid scenario fixtures. The hourglass (⏳) and
tea-cup (🍵) emoji deliberately stay outside this subset and use LVGL's missing-glyph placeholder;
emoji rendering is not covered by the host text-quality contract.
"""

import hashlib
import json
import re
import subprocess
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/fonts/host_zh_14.c"
SCENARIOS = ROOT / "tests/scenarios"
SOURCE = (
    "https://raw.githubusercontent.com/google/fonts/"
    "7085eb89a950e85db5b166b7a58d414544b4140c/"
    "ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf"
)
EXPECTED_SHA256 = "a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da"
EMOJI = {"⏳", "🍵"}


def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for key, item in value.items():
            yield from strings(key)
            yield from strings(item)
    elif isinstance(value, list):
        for item in value:
            yield from strings(item)


def required_symbols() -> str:
    symbols = set()
    for path in sorted(SCENARIOS.glob("*.json")):
        if path.name.startswith("_bad_"):
            continue
        fixture = json.loads(path.read_text(encoding="utf-8"))
        for text in strings(fixture):
            symbols.update(char for char in text if ord(char) > 0x7F and char not in EMOJI)
    return "".join(sorted(symbols))


def main() -> None:
    try:
        from fontTools.ttLib import TTFont
        from fontTools.varLib import instancer
    except ImportError as error:
        raise SystemExit("gen_host_font.py requires Python fonttools") from error

    symbols = required_symbols()
    with tempfile.TemporaryDirectory() as directory:
        variable = Path(directory) / "NotoSansSC-VF.ttf"
        static = Path(directory) / "NotoSansSC-w600.ttf"
        urllib.request.urlretrieve(SOURCE, variable)
        digest = hashlib.sha256(variable.read_bytes()).hexdigest()
        if digest != EXPECTED_SHA256:
            raise SystemExit(f"unexpected Noto Sans SC SHA-256: {digest}")
        font = TTFont(variable)
        instancer.instantiateVariableFont(font, {"wght": 600}, inplace=True)
        font.save(static)
        subprocess.run(
            [
                "npx", "--yes", "lv_font_conv@1.5.3", "--font", str(static),
                "--range", "0x20-0x7E", "--symbols", symbols, "--size", "14",
                "--bpp", "4", "--no-compress", "--format", "lvgl",
                "--lv-include", "lvgl.h", "--lv-font-name", "host_font_zh_14",
                "-o", str(OUT),
            ],
            check=True,
        )
    generated = OUT.read_text(encoding="utf-8")
    generated = re.sub(r"--font \S+NotoSansSC-w600\.ttf", "--font NotoSansSC-w600.ttf", generated)
    generated = re.sub(r"-o \S*assets/fonts/host_zh_14\.c", "-o assets/fonts/host_zh_14.c", generated)
    OUT.write_text(generated.rstrip() + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} with {len(symbols)} fixture symbols (emoji excluded)")


if __name__ == "__main__":
    main()
