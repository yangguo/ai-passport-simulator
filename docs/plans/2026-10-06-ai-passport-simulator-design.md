# AI Passport Simulator Design

**Status:** Initial design for review and implementation planning; no simulator code exists yet.

## Problem and outcome

Passport UI and interaction changes currently require a firmware build and a physical device to inspect every visual state. The project now needs adaptive subtitle layout, separate user/assistant caption retention, state transitions, and repeatable network/audio failure handling. A narrow desktop test environment should make those cases quick to replay while keeping real-device claims separate.

## Options considered

### A. Full firmware under ESP32-C3 QEMU/Wokwi

This could exercise more of the target runtime, but it would require mapping Passport's ST7789/SPI path to an emulator display and supplying substitutes for its ES8311, ADC keys, CW2017, and board power behavior. Even a successful boot would not validate the absent physical peripherals or real Wi-Fi path. It is too broad for the first milestone.

### B. Independent desktop mock UI

This would be quick to draw, but it risks visual and behavior drift because it duplicates Passport's LVGL widgets and layout rules. It may be useful for a throwaway mock, but not as the project test authority.

### C. Host LVGL harness with shared portable behavior — recommended

Run LVGL 9.5.x in an SDL2 desktop window at 240×320. Share or extract small portable modules for geometry, activity state, subtitle storage, and pagination. Use desktop adapters and scripted events for controls and services. This targets the most frequent changes without claiming to emulate electrical hardware.

## Architecture

The desktop shell owns SDL windowing, keyboard/mouse input, screenshot capture, and an event log. A deterministic scenario engine loads versioned JSON events and advances a virtual clock. The Passport presentation module owns view state and bounded separate user/assistant caption buffers. LVGL renders the same core widgets and geometry rules used by the firmware where practical. Mock services generate normalized button, STT/TTS, transport, audio-queue, battery, settings, and power events. The complete architecture and dependency constraints are in `../ARCHITECTURE.md`.

The first implementation must inspect the firmware source before extracting code. Candidate shared sources are `passport_activity.h`, `screen_rounding.c/.h`, and a new caption/state module. Do not link the board class or whole Application into the host application. Do not create duplicate UI rules when a thin platform adapter can share the behavior.

## Core display and conversation behavior

- Idle/Listening: centered expression remains the visual focus.
- Thinking/Speaking: expression shrinks and moves into the top safe area; subtitle area expands below the status bar.
- User and assistant captions occupy separate bounded buffers.
- Valid STT replaces only the previous user utterance. TTS events and state changes do not erase it. Empty/invalid STT is ignored.
- New TTS start resets the assistant response only; sentence events update that response.
- Long strings wrap/page in the 30 px rounded-glass safe area and truncate on valid UTF-8 boundaries.
- Virtual UP/DOWN/OK actions use explicit press/release events and do not claim ADC fidelity.

## Failure and reset behavior

Invalid scenario files fail before playback begins. Unknown event versions/types are rejected. A scenario reset restores virtual time, mock stores, captions, page timers, state, and log. Disconnect, timeout, packet gaps/reorder, and queue saturation are controlled fixture events; the simulator must visibly distinguish them from measured network/audio facts. SDL failure must not prevent headless unit tests from running.

## Test and acceptance approach

Unit tests cover geometry, text buffers, state transitions, event validation, and virtual key sequences. Scenario tests cover the complete STT/TTS lifecycle, interruption, long text, and injected failure recovery. Golden screenshots cover both layouts and key overlays. CI runs deterministic host tests and screenshot comparisons; failure artifacts include actual/expected/diff images. Firmware builds and physical-device tests remain separate gates.

The first usable release is accepted only when it builds on clean macOS and Ubuntu checkouts, replays scenarios deterministically, preserves user STT through an assistant turn, keeps all UI inside the glass safe area, and clearly lists hardware behavior it cannot verify.

## Open decisions before implementation

- Pin the exact LVGL 9.5.x version resolved by the target firmware lock/build.
- Confirm which existing Passport UI widgets and assets may be reused and their redistribution licenses.
- Choose a portable unit-test framework and image comparison threshold.
- Decide repository source license before accepting third-party contributions or publishing binaries.
- Decide whether P0-01 may require dependency download on first configure or must be fully offline after bootstrap.

## Implementation plan

The phased tasks, files, validation commands, and exit gates are in `../ROADMAP.md`. Initial planned host commands are in `../DEVELOPMENT.md`; they must not be described as working until a clean-checkout build verifies them.
