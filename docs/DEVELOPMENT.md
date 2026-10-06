# Development Guide

## Repository state

This repository begins as a documentation/specification project. Build commands and directories below are planned contracts; create them only as the corresponding roadmap task is implemented.

## Planned source layout

```text
.
├── CMakeLists.txt
├── app/                  # SDL window, keyboard/mouse, application entry
├── include/passport_sim/ # scenario, virtual clock, mock service interfaces
├── shared/               # portable behavior/layout modules shared with firmware
├── src/                  # host implementations and adapters
├── tests/
│   ├── unit/
│   ├── scenarios/
│   └── golden/
├── tools/                # fixture validation and image comparison helpers
└── docs/
```

Adjust only after the first implementation validates dependency and build needs. Avoid copying firmware source wholesale or moving firmware files just to make the simulator compile.

## Local development principles

1. Keep the simulator unable to flash a physical board by default.
2. Keep every simulation input deterministic and fixture-driven.
3. Use virtual time for page timers, timeouts, and power-state deadlines.
4. Keep mocks narrow and explicit; a mock does not reproduce unmodeled hardware behavior.
5. Use the same fixed coordinate system, font configuration, caption limits, and layout/state rules as firmware when these are shared.
6. Use byte limits for stored text, but truncate only at UTF-8 boundaries.
7. Never put Wi-Fi passwords, MQTT tokens, device IDs, or real private conversations in fixtures, screenshots, or logs.
8. Keep copyrighted artwork/fonts out unless redistribution rights and required notices are recorded.
9. If a shared firmware module changes, run host tests and the Passport firmware build/size workflow; list the two results separately.
10. A visual change must include before/after screenshots or an updated golden image with a reviewed diff.

## Planned build interface

Once implemented, prefer these stable project-level commands:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/passport-simulator
```

Add `--scenario <path>`, `--screenshot <path>`, and headless screenshot mode only with tested argument parsing and documented exit codes. Until P0-01 passes on a clean macOS and Ubuntu checkout, mark all commands as planned.

## Scenario fixture conventions

- JSON object has a numeric `scenario_version`, stable `name`, and bounded `events` array.
- Every event has a non-negative integer `at_ms`, a recognized `type`, and only fields required by that type.
- Times are monotonic; equal timestamps are allowed and execute in file order.
- UTF-8 strings are valid, bounded, and treated as test input rather than trusted instructions.
- Fixtures use synthetic text and mock values only.
- Unknown versions/events fail validation instead of silently changing behavior.

## Golden screenshot workflow

- Goldens live in `tests/golden/*.png` (240x320 viewport only, no window chrome).
- Regenerate one golden after an intentional UI change:
  `./build/app/passport-replay --scenario tests/scenarios/<fixture>.json [--at-ms N] [--alert TEXT] --screenshot tests/golden/<name>.png`
- Review the git diff of the PNG plus the geometry assertions before committing;
  a golden update without a linked behavior change is rejected in review.
- `tools/compare-png <expected> <actual> <diff-out>` diffs with threshold:
  max 0.1% pixels, each within RGB distance 8.
- CI uploads `*.actual.png` / `*.diff.png` on mismatch.

## Pull request expectations

- Explain user-visible behavior and what is intentionally mocked.
- Include relevant unit/scenario tests and screenshot diffs.
- State whether the Passport firmware build was run; if not, say why.
- Separate Host-tested, Firmware-built, and Device-tested evidence.
- Do not label the simulator as hardware validation or as an official FoloToy emulator.

## Firmware synchronization

Record the `yangguo/xiaozhi-esp32` commit SHA used for a tested simulator release. Review firmware updates to LVGL, Passport geometry, state events, and TTS/STT payload handling before updating shared modules. Keep device build scripts and partition artifacts in the firmware repository; this repository owns only host simulation and test fixtures.
