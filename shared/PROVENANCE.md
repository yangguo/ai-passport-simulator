# Provenance of imported firmware files (all MIT-licensed)

## `shared/screen_rounding.h` / `shared/screen_rounding.c`

- Byte-identical copy of
  `yangguo/xiaozhi-esp32:main/boards/folotoy/ai-passport/screen_rounding.*`.
- Upstream origin: `FoloToy/ai-passport:components/bsp/src/bsp_display_rounding.*`.
- Recorded divergence (2026-10-06): the firmware copy renamed the
  `bsp_display_` prefix to `passport_`, dropped
  `bsp_display_pixel_outside_rounded_rect`, and added safe-rect, subtitle
  viewport, activity-line, paging, widget-placement, and RGB565-mask helpers.
  The official two-function API is a strict subset in behavior for the shared
  `rounded_row_span` core; geometry tests assert the official corner
  semantics (row 0 visible from x=30 at 240x320 r30).
- Rule: never edit these files. Record further divergences here.

## `shared/passport_activity.h`

- Byte-identical copy of
  `yangguo/xiaozhi-esp32:main/boards/folotoy/ai-passport/passport_activity.h`,
  except the single adapted include line (`device_state.h` ->
  `device_state_shim.h`, host-only enum copy with identical values).
