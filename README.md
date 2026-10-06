# AI Passport Desktop Simulator

AI Passport Desktop Simulator is a planned desktop test environment for developing and checking the UI and interaction logic of the FoloToy AI Passport firmware. It is intended to shorten the edit-build-flash cycle for display layout, subtitles, buttons, conversation states, settings, and repeatable failure scenarios.

The first version is a **desktop simulation and test harness**, not a hardware emulator. It will render at the device's 240×320 resolution, reproduce the 30 px rounded-glass mask, accept virtual UP/DOWN/OK input, and inject deterministic STT/TTS, network, audio-queue, battery, and settings events. Hardware-specific claims must still be checked on a real Passport.

## Status

P0 simulator implemented and green on macOS (clean-checkout configure, build,
and all 15 tests pass) with Ubuntu CI configured. See the
[device checklist](docs/DEVICE_CHECKLIST.md) for what simulation cannot verify.

## Build, test, and run (verified on macOS)

Prerequisites: `brew install cmake ninja pkg-config sdl2` (first configure
downloads pinned LVGL/doctest/JSON/stb sources automatically).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/app/passport-simulator --scenario tests/scenarios/ptt-normal.json
./build/app/passport-replay --scenario tests/scenarios/stt-retained-through-tts.json --screenshot out.png
```

`passport-replay` exit codes: 0 ok, 2 bad usage, 3 invalid fixture.
`--screenshot` renders the 240×320 viewport headlessly; add `--at-ms N` for a
checkpoint and `--alert TEXT` for an alert overlay. Goldens live in
`tests/golden/`; see [Development guide](docs/DEVELOPMENT.md) for the update
workflow.

## Project documents

- [Requirements](docs/REQUIREMENTS.md): goals, scope, functional/non-functional requirements, acceptance criteria.
- [Architecture](docs/ARCHITECTURE.md): modules, boundaries, event and data flow, desktop/firmware sharing strategy.
- [Test strategy](docs/TEST_STRATEGY.md): unit, scenario, screenshot, CI, and real-device checks.
- [Implementation roadmap](docs/ROADMAP.md): prioritized milestones and exit gates.
- [Development guide](docs/DEVELOPMENT.md): source layout, build conventions, fixture and contribution rules.
- [Design plan](docs/plans/2026-10-06-ai-passport-simulator-design.md): reviewed initial design and assumptions.

## Recommended first release

1. Build a macOS/Linux SDL2 desktop window at 240×320 using the same LVGL major/minor version as the Passport firmware.
2. Reuse Passport's portable display geometry and conversation-state logic; avoid maintaining a visually similar but unrelated mock UI.
3. Add keyboard controls and a deterministic scenario runner for STT/TTS/state changes.
4. Add image snapshots and regression tests for the two subtitle layouts and user-text retention.
5. Add mocked network, audio-queue, battery, NVS, and power events after the display/input path is stable.

The simulator must label mocked signals as simulated and must never report simulated audio, network, battery, or power results as hardware verification.

## Implemented technology (P0 pins)

- C++17 host application and CMake (3.28+)
- LVGL v9.5.0 (`85aa60d`), SDL release-2.32.10, doctest v2.5.3,
  nlohmann/json v3.12.0, stb image I/O (`2c980bb`) — all fetched automatically
- CTest host tests + `compare-png` golden regression; GitHub Actions on Ubuntu
- Fonts (P0): built-in Montserrat 14 only — CJK renders as placeholder boxes
  until a licensed CJK font lands; see the device checklist

This repository does not yet select a software license; make that decision before accepting code contributions or redistributing firmware-derived assets.

## Relationship to firmware

The firmware source of truth is [yangguo/xiaozhi-esp32](https://github.com/yangguo/xiaozhi-esp32), especially `main/boards/folotoy/ai-passport`. Changes intended to affect device behavior must be shared through small portable modules or explicit adapters and validated against the firmware build. The simulator is not a replacement for the real firmware, its build, or device testing.
