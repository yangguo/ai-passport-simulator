# Architecture

## 1. Design decision

Build a desktop host harness around the Passport UI and portable behavior logic. Use SDL2 for the desktop window/input and LVGL 9.5.x for rendering, pinned to the reference firmware's resolved LVGL release. Extract only the small modules needed to share real behavior—starting with rounded-screen geometry, subtitle state/buffering, and activity transitions. Keep ESP-IDF, board drivers, and physical peripheral code outside the host executable.

This is preferred over a first-release whole-chip emulator. ESP32-C3 QEMU has a virtual framebuffer path that would require replacing the Passport ST7789 path, and desktop execution still would not provide an ES8311, physical buttons, CW2017, real Wi-Fi route, or battery measurements. QEMU/Wokwi may remain optional integration experiments.

## 2. Layer diagram

```text
┌─────────────────────────────────────────────────────────────┐
│ Desktop shell: window, keyboard/mouse, toolbar, event log    │
├─────────────────────────────────────────────────────────────┤
│ Scenario runner: JSON fixtures, virtual clock, replay       │
├─────────────────────────────────────────────────────────────┤
│ Passport presentation: shared layout, captions, state model │
├────────────────────────────┬────────────────────────────────┤
│ LVGL + SDL display/input   │ Virtual device services       │
│ 240×320, glass mask        │ buttons/network/audio/NVS/... │
├────────────────────────────┴────────────────────────────────┤
│ Firmware adapters (not linked into host)                     │
│ ESP-IDF Application / PassportDisplay / board / peripherals  │
└─────────────────────────────────────────────────────────────┘
```

## 3. Components

### Desktop shell

- Creates an SDL2 window with device bezel and 240×320 viewport.
- Maps Up/Down/Enter to virtual UP/DOWN/OK; mouse click targets the same virtual button interface.
- Provides scenario controls, simulated/real label, screenshot capture, and event log.
- Uses no physical serial/GPIO/audio device unless a future opt-in feature is separately designed.

### Passport presentation module

- Owns presentation state: Idle, Listening, Thinking, Speaking, Error, Menu.
- Owns separate bounded user and assistant caption buffers and transition rules.
- Computes the idle/listening and caption-priority viewports from device resolution, radius, status reserve, and measured font line height.
- Reuses portable firmware code where feasible. If a UI element cannot be shared, record the divergence and add a firmware-side test instead of silently maintaining two behaviors.

### Virtual device services

Each service implements a narrow interface and labels its behavior as simulated:

- `ButtonInput`: press/release/hold/short-click sequences.
- `ConversationEvents`: STT/TTS/error/activity inputs.
- `TransportMock`: connected/disconnected/timeout and packet-sequence symptoms.
- `AudioMock`: queue depth, packet interval, drop counter, optional waveform visualization only.
- `BatteryMock`: percentage, absent gauge, low-battery threshold.
- `SettingsStoreMock`: simulator-only persistence.
- `PowerMock`: dim/soft-sleep/deep-sleep state transitions driven by virtual time.

The interfaces should not duplicate all ESP-IDF APIs. Add only the methods required by a verified scenario.

### Scenario runner

Loads a versioned event list, validates bounds, and schedules events against one deterministic virtual clock. The runner has play/pause/step/reset operations. A test can advance directly to the next event rather than waiting for wall-clock time. Timers that are intentionally visual may run at an adjustable speed while preserving the scenario's virtual timestamps.

Initial JSON shape:

```json
{
  "scenario_version": 1,
  "name": "stt-retained-through-tts",
  "events": [
    {"at_ms": 0, "type": "state", "value": "listening"},
    {"at_ms": 450, "type": "stt", "text": "明天下午天气怎么样？"},
    {"at_ms": 700, "type": "state", "value": "thinking"},
    {"at_ms": 1200, "type": "tts_start"},
    {"at_ms": 1400, "type": "tts_sentence", "text": "明天下午可能会下雨。"},
    {"at_ms": 2500, "type": "tts_stop"}
  ]
}
```

This is a test fixture contract, not the XiaoZhi network protocol. Protocol-specific fixtures must remain separate and must not be sent to production servers by default.

## 4. Data flow

1. SDL input or scenario event enters a virtual service.
2. The service emits a normalized event into the shared presentation/state module.
3. The module updates bounded caption/state data and asks LVGL to render.
4. Scenario runner records the event and resulting state with virtual time.
5. Screenshot tests capture the viewport after stable checkpoints.

Hardware-side adapter maps Application/Protocol/board events into the same normalized behavior only after the shared module is proven. The simulator must not become a second source of truth for firmware behavior.

## 5. Error handling

- Invalid scenario: show file path, event index, and validation reason; do not partially execute unless a future explicit recovery mode is added.
- Unknown event or version: reject the fixture without changing state.
- Oversized text/event list: enforce the same configured limits as the shared module and log the truncation/rejection.
- SDL/window failure: exit with a clear error; unit tests should still run headlessly.
- Missing optional assets/font: use a documented placeholder and surface a warning; never substitute copyrighted art silently.
- Scenario reset: restore all mock state, clock, captions, logs, and page timers to their initial state.

## 6. Build and dependency approach

- CMake host build; C++17 or the repository's confirmed minimum standard.
- SDL2 and exact LVGL 9.5.x dependency are pinned for reproducible builds.
- Avoid depending on a global ESP-IDF environment for host tests.
- Pin fonts/assets or use redistributable test assets; document their license and provenance.
- First run on macOS locally and Ubuntu CI. Add Windows after the SDL/CI path is stable.

## 7. Source sharing and divergence control

Before extracting firmware code, inspect `main/boards/folotoy/ai-passport/passport_display.*`, `screen_rounding.*`, `passport_activity.h`, and the source of subtitle events in `main/application.cc`. Prefer small portable C/C++ modules with no FreeRTOS, ESP-IDF, LVGL-port, or board-object dependencies. Keep board-specific LVGL creation in a thin adapter. For every shared rule, add a test that runs from both host and firmware build targets when practical.

Do not copy the entire `xiaozhi-esp32` repository into this project. Record the firmware source repository and tested commit in simulator releases; synchronize deliberately and review changes to its LVGL version, subtitle event contract, and Passport display API.
