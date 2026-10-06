# Device Evidence (firmware backup 2026-10-06)

Source: `/Users/vyang/Desktop/spaces/xiaozhi-esp32/backups/ai-passport/2026-10-06`
(captured from the physical Passport; kept in the firmware checkout, NOT in
this repo). Values below are hashes and key NAMES only — never credentials.

## Image inventory (SHA-256)

- `passport-before-firmware-full-8mb.bin` (8 MiB):
  `9d630a46e8fe23127a5d08111fee87ca3ebb146917543caf960ff7beafea7dcb`
- `passport-before-d4736e7-full-8mb.bin` (8 MiB):
  `a8f2fd7cade2cf82966700dbcfb3e3e042187298ec21ae9da8fffa75ce74e04b`
- NVS snapshots (16 KiB each): repeated reads are byte-identical
  (`passport-before-d4736e7-nvs-16kb.bin` ==
  `passport-before-d4736e7-nvs-reread.bin`), and NVS is byte-identical
  across the d4736e7 operation (before == after) — settings survive the
  procedure on device.

## Firmware build under test (`firmware/`)

- Partitions: bootloader@0x0, partition-table@0x8000, app@0x20000 (2.37 MB),
  assets@0x600000; flash 8 MB dio 80m, ESP32-C3, no PSRAM.
- Size gate: PASS — flash 2,290,294 B used; DRAM 109,716 B used,
  211,580 B remain (static linker estimate, not measured free heap).
- Binary contains LVGL init paths, `LvglDisplay` symbols, `listening` /
  `thinking` status strings, and brightness handling with default 10 —
  matching `kPassportDimBrightness = 10` shared by the simulator.

## NVS key names observed (values redacted)

Namespaces/keys include: `display`/`theme`, `brightness`, `output_volume`,
`uuid`, `mqtt` (`endpoint`, `publish_topic`, `subscribe_topic`), `websocket`
(`url`), Wi-Fi STA credentials. The simulator `SettingsStore` stub models
`brightness`-style keys in memory only and never reads these dumps.

## What this evidence does and does not prove

- Proves: the firmware image under test, its resource headroom (the 512 B /
  2 KiB caption limits are three orders of magnitude below DRAM remain),
  NVS read stability, and settings preservation across the recorded
  operation.
- Does NOT prove: any simulator screenshot equals device rendering, audio or
  network behavior, or power/battery claims. Those need live device runs —
  see [DEVICE_CHECKLIST.md](DEVICE_CHECKLIST.md).
