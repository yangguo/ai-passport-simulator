# Xiaoxiaole digit-font handoff

This is an upstream handoff for the bot maintaining `yangguo/ai-passport-xiaoxiaole`. No files in
that repository are changed by this work.

## Observed state

- `main/xxl_ui.c` assigns `xxl_font_zh_16` to its user-visible labels, and
  `main/xxl_board_draw.c` uses it for board text. The generated font includes ASCII `0x20–0x7E`.
- Intro UI-tree screenshots already show digits in their label text, so the missing digits are a
  rendered-font selection/fallback issue rather than absent tree text.
- `main/app_main.c::show_error` explicitly selects `lv_font_montserrat_14`.
- `sdkconfig.defaults` enables Montserrat 14 and 20 as LVGL defaults.

## Minimal upstream change

1. Set the LVGL theme/default text font to `xxl_font_zh_16` immediately after LVGL initialization,
   so widgets without an explicit per-label font do not fall back to Montserrat. Keep the explicit
   `xxl_font_zh_16` assignments on the existing screen helpers.
2. In `main/app_main.c::show_error`, change the label font from `lv_font_montserrat_14` to
   `xxl_font_zh_16`.
3. Run `tools/check_font_coverage.py`, rebuild the firmware, publish a new artifact, and report its
   workflow run ID plus merged-bin and ELF SHA-256 values so this simulator can pin a new corpus
   identity.

Do not change the simulator golden to hide tofu. `text_quality` remains failing until the new
firmware artifact is pinned and its digits render correctly.
