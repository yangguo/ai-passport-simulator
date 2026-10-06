# Implementation Roadmap

This roadmap covers the simulator project only. It does not mark any feature implemented until code, tests, and evidence exist.

## P0 — useful host UI and deterministic behavior

### P0-01 Build skeleton

- Create CMake project, pin C++ standard, SDL2, and LVGL 9.5.x.
- Add a 240×320 window and headless test configuration.
- Add CI compile/test workflow for Ubuntu.
- Exit gate: clean checkout configures/builds/tests using documented commands; version and dependency provenance are recorded.

### P0-02 Shared Passport layout and captions

- Inspect and extract the smallest portable pieces from `passport_display.*`, `screen_rounding.*`, and `passport_activity.h`.
- Implement device coordinate space, round-mask rendering, status safe-area, current 2.5 s page behavior fixture, and adaptive caption-priority layout.
- Add separate user/assistant buffers with user STT retention and UTF-8-safe bounds.
- Exit gate: unit tests plus reviewed screenshots for both layouts, mixed text, long text, and STT/TTS lifecycle.

### P0-03 Virtual controls and scenario playback

- Add keyboard/mouse UP/DOWN/OK input using press/release events.
- Add versioned JSON fixture schema, virtual clock, play/pause/step/reset, logs, and screenshots.
- Add PTT, menu, TTS interruption, and state-flow scenarios.
- Exit gate: repeated playback has identical event log and screenshots.

### P0-04 CI regression harness

- Add headless CTest/scenario/screenshot jobs and failure artifacts.
- Add test-data/license inventory and contribution instructions.
- Exit gate: a deliberate layout/state regression fails CI with a useful diff.

## P1 — subsystem mocks

### P1-01 Transport and audio-queue test doubles

- Inject disconnect, timeout, packet loss/gap/reorder, and queue saturation into normalized events.
- Display mock queue/sequence counters and test user-visible recovery prompts.
- Do not claim RF or codec accuracy.

### P1-02 Settings/Profile transaction mock

- Model settings and server/agent profile changes in a simulator-only store.
- Exercise validation, pending/commit/rollback, unsupported capability, and persistence across app restart.
- Keep authentication secrets out of fixtures and logs.

### P1-03 Battery/power timeline mock

- Drive dim, soft-sleep, deep-sleep, missing-battery, and wake-key transitions with virtual time.
- Report the output as state-machine behavior only, not current consumption or physical wake reliability.

### P1-04 Optional Wokwi/QEMU spike

- Time-box an investigation of ESP32-C3 graphics or peripheral smoke testing.
- Keep it separate from the desktop LVGL product and document exact supported peripherals.
- Continue only if the same firmware behavior is tested without invasive board-specific rewrites.

## P2 — quality-of-life and wider use

- Windows build/test support.
- Scenario editor UI, reusable scenario gallery, and side-by-side screenshot comparison.
- Optional local mock server for WS/MQTT control-message contract tests; never enabled by default.
- Evaluate controlled audio-file playback fixtures if licensing and codecs permit.
- Additional Passport resolutions/board adapters only with a concrete maintainer and test hardware.

## Release 0.1 exit checklist

- P0-01 through P0-04 exit gates pass.
- At least ten scenarios run in CI and produce deterministic artifacts.
- Docs distinguish simulator limits and give a short physical-device acceptance checklist.
- A source commit and reproducible desktop build are recorded.
- License, asset provenance, and dependency notices are decided before public binary redistribution.
