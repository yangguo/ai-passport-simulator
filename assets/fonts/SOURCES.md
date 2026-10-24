# Host font assets

`host_zh_14.c` is a 14 px, 4 bpp subset of Noto Sans SC at weight 600, generated from the Google
Fonts variable font pinned at commit `7085eb89a950e85db5b166b7a58d414544b4140c`:

- Source: `https://raw.githubusercontent.com/google/fonts/7085eb89a950e85db5b166b7a58d414544b4140c/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf`
- Source SHA-256: `a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`
- License: SIL Open Font License 1.1 (`OFL.txt`)
- Generator: `python3 tools/gen_host_font.py`, Python `fonttools`, and `npx lv_font_conv@1.5.3`
- Glyph inventory: ASCII `0x20–0x7E` and all non-ASCII codepoints in valid `tests/scenarios/*.json`; `_bad_*` fixtures are excluded.
- Emoji policy: `⏳` and `🍵` are excluded and display LVGL's missing-glyph placeholder. Emoji are not part of host text-quality coverage.
- Generated C SHA-256: `8bbf093ac7800d7325aca78cedf37d1610aa4e1c0bd6620d9f4a21fdcfe0b1da`

Run `python3 tools/check_host_font.py` to verify generated glyph coverage against the fixture inventory.
