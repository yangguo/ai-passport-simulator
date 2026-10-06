# Requirements

## 1. Purpose

Provide a repeatable desktop environment for developing and checking AI Passport firmware behavior without flashing the physical device for every visual or event-flow change. The initial target is Passport subtitle work: the two display modes and retention of the latest recognized user utterance while the assistant responds.

The simulator should grow into a useful test harness for button routing, conversation states, settings/profile flows, and injected transport failures. It is not intended to emulate the analog, acoustic, RF, or energy behavior of the physical board.

## 2. Target device profile

The reference device is the FoloToy AI Passport:

- MCU: ESP32-C3; 8 MB flash; no PSRAM.
- Display: ST7789, portrait 240×320, rounded glass with a 30 px corner radius.
- Audio: ES8311 codec on I2C and I2S full-duplex audio.
- Battery: optional CW2017 gauge.
- Controls: UP, DOWN, and OK share GPIO0 through an ADC resistor ladder.
- Firmware UI: LVGL 9.5.x as resolved by the reference firmware; confirm and pin the exact version before implementation.

The desktop viewport must preserve device pixel dimensions for layout calculations. Optional zoom may enlarge the window but must not alter the virtual coordinate system.

## 3. Users and core use cases

### Primary users

- Firmware developers iterating on Passport UI and interaction logic.
- Testers reproducing known input, subtitle, and state-transition sequences.
- Reviewers comparing visual behavior before and after a change.

### Core use cases

1. Preview Idle/Listening and Thinking/Speaking layouts at the exact 240×320 coordinate size.
2. Inject a user STT result, assistant TTS start/sentence/stop events, and observe what remains visible.
3. Navigate with virtual UP/DOWN/OK controls and exercise short/long press routes.
4. Replay a saved event scenario deterministically, pause it, step through events, reset it, and capture screenshots.
5. Inject mock transport symptoms such as disconnect, timeout, sequence gap, packet reorder, and queue overflow; inspect UI recovery and event logs.
6. Exercise fake settings/profile storage across simulator restart without touching physical-device NVS.

## 4. In-scope functional requirements

### F-01 Device screen

- Render a 240×320 device viewport and mask the outer 30 px rounded corners.
- Show the status row, activity state, expression/avatar placeholder, subtitle area, settings list, and low-battery/error presentation used by the Passport UI.
- Keep mask geometry and text safe-area decisions shared with, or generated from, firmware logic.
- Provide a zoomed desktop window while preserving 1:1 virtual coordinates and screenshots.

### F-02 Adaptive subtitle layouts

- Idle/Listening mode keeps the centered expression layout.
- Thinking/Speaking mode switches to a caption-priority layout: shrink and move the expression into the top safe area and give subtitles the remaining safe height.
- Never draw text outside the rounded-glass safe area or over the status bar and alert region.
- Keep “我” (latest valid STT) and “AI” (current assistant response) in separate view regions/buffers.
- A valid STT replaces the previous user utterance. TTS start, sentence updates, state changes, and empty system messages must not erase it.
- A new TTS turn resets only the assistant buffer. Empty or invalid STT does not replace the previous valid user text.
- Support wrapped and paged long text. Text limits must be configurable constants shared with tests; proposed starting limits are 512 UTF-8 bytes for user text and 2 KiB for assistant text, subject to firmware heap measurements.
- Show truncation explicitly and never split a UTF-8 code point.

### F-03 Virtual controls

- Map keyboard Up/Down/Enter (and optionally configurable keys) to UP/DOWN/OK.
- Show visible virtual buttons that can be clicked.
- Support press, release, short press, long press, and repeated press sequences.
- Make virtual controls feed the same input/state logic used by the firmware where practical; do not claim ADC calibration or electrical debounce is simulated.

### F-04 Deterministic scenario playback

- Load versioned JSON scenario files.
- Provide play, pause, single-step, reset, and selectable playback speed.
- Use a virtual monotonic clock for all scenario-driven events and timers.
- Record event name, virtual timestamp, and resulting UI/state changes in a readable log.
- Reject malformed, oversized, unknown-version, and out-of-order scenario data with actionable errors.

### F-05 Mock services

- Mock application events for STT, TTS, errors, listening mode, and conversation state.
- Mock transport availability and injected MQTT/UDP symptoms; do not connect to a real service by default.
- Mock audio queue counters and packet timing; no live microphone or physical codec is required.
- Mock battery level/unknown state and power-state transitions on virtual time.
- Mock settings/Profile persistence in a simulator-owned file or in-memory store; never access physical NVS.
- Clearly label every mocked service and value in the simulator UI/log.

### F-06 Reproducible review artifacts

- Capture the 240×320 viewport to PNG on demand and from scenarios.
- Provide stable golden-image tests for selected screens, with documented update/review steps.
- Include scenario files for normal PTT, long assistant response, repeated STT, empty STT, TTS interruption, disconnect/timeout, packet gap, queue overflow, missing battery, and menu entry/exit.

## 5. Non-functional requirements

- **Determinism:** same build, fixture, and virtual-clock settings produce the same event sequence and screenshot.
- **Firmware alignment:** pin LVGL and document any rendering difference from target firmware; share layout/state logic rather than duplicating rules.
- **Portability:** macOS and Ubuntu are first-class developer/CI targets. Windows is a later milestone.
- **Safety:** simulator operations cannot flash or alter a connected Passport. No real device serial port is opened by default.
- **Privacy:** no microphone capture, external service calls, telemetry, Wi-Fi credentials, tokens, or conversation upload in the initial design.
- **Usability:** one command should eventually build and launch the desktop app; one command should run tests. Commands remain provisional until implementation.
- **Maintainability:** keep mocks behind interfaces; avoid emulating peripherals not needed by a test case.

## 6. Explicit non-goals and fidelity boundaries

The simulator is not an exact digital twin. The initial and foreseeable desktop releases do not claim to validate:

- ES8311 register behavior, microphone sensitivity, amplifier output, speaker response, acoustic echo, or real audio quality.
- ADC resistor-ladder voltages, key noise, real button debounce, or wake-on-GPIO electrical timing.
- Real Wi-Fi RF behavior, router/proxy/NAT paths, UDP jitter/loss distributions, server availability, or service credentials.
- CW2017 electrical measurements, charging state, battery capacity, current consumption, or runtime estimates.
- ESP32-C3 heap/stack pressure, DMA allocation limits, flash partition behavior, secure boot, OTA rollback, deep-sleep current, or reset behavior.
- The Passport's physical glass, viewing angle, LCD color/contrast, backlight leakage, or exact rounded-mask appearance under the lens.
- Booting the unmodified complete Passport firmware on a desktop simulator.

QEMU/Wokwi experiments may be evaluated later, but they must not block the host UI/event harness and must retain explicit peripheral coverage notes.

## 7. Acceptance criteria for the first usable release

1. A clean macOS and Ubuntu checkout can configure, build, and run the desktop simulator using documented commands.
2. The viewport reports 240×320 virtual pixels and produces a correctly rounded 30 px-mask screenshot.
3. Idle/Listening and Thinking/Speaking layouts both render inside the glass safe area, with no overlap among status, expression, user caption, assistant caption, and alerts.
4. A deterministic fixture proves user STT survives TTS start, multiple TTS sentences, Speaking→Idle, and empty system messages, then is replaced only by the next valid STT.
5. Long Chinese/Latin mixed text, emoji/missing-glyph handling, truncation, and pagination have unit and screenshot coverage.
6. Virtual key press/release/long-press flows reproduce the documented UI behavior without accessing GPIO or a serial device.
7. At least the documented scenarios replay with identical event logs and golden screenshots on repeat runs of the same CI image.
8. Network/audio/battery/power mock values are visibly marked simulated and cannot be confused with real hardware measurements.
9. CI runs host tests and screenshot regression tests; firmware build remains a separate required check for shared firmware changes.
10. A short real-device checklist states what remains unverified by simulation.
