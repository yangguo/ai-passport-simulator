# AI Passport Simulator P0 Design (spec)

- Date: 2026-10-06
- Scope: P0-01 + P0-02, phased vertical slice (approach B), ending with real LVGL rendering
- Status: brainstormed and user-approved; pending spec review, then implementation plan

## 1. Decisions already made

- First-round scope: full P0-02 including real LVGL rendering (Idle/Listening + Thinking/Speaking layouts, screenshots).
- Delivery order (approach B): ① skeleton + portable core with SDL stub, fully tested; ② swap in real LVGL/SDL rendering + layouts + golden screenshots. Same milestone, two-step merge to de-risk the LVGL/SDL bring-up.
- Dependencies: CMake FetchContent auto-fetch (first configure needs network). Pin LVGL v9.5.0 (matches `lvgl/lvgl ~9.5.0` / `^9.5.0` in both firmware sources; confirm resolved patch from `dependencies.lock` during P0-01).
- Unit-test framework: doctest (single header, lightest FetchContent).
- Toolchain: allowed to `brew install cmake ninja pkg-config sdl2` on the dev Mac.
- Firmware alignment source: local checkout at `/Users/vyang/Desktop/spaces/xiaozhi-esp32` (+ backups under `backups/ai-passport/2026-10-06` are flash `.bin` only, not source).

## 2. Firmware / official reference map

- Geometry source of truth: `FoloToy/ai-passport` `components/bsp/src/bsp_display_rounding.{h,c}` (pure C, host-testable by design). Downstream copy `xiaozhi-esp32:main/boards/folotoy/ai-passport/screen_rounding.*` must be diffed; divergences recorded in the spec/PR, official wins.
- Activity logic: `xiaozhi-esp32:.../ai-passport/passport_activity.h` reused with a small `device_state_shim.h` (only the `DeviceState` enum values it needs). Board class `PassportDisplay` (730-line `.cc`, depends on `SpiLcdDisplay` + LVGL widgets + ESP-IDF) is NOT linked into the host; a thin adapter maps events instead.
- Button semantics: official `bsp_button.h` event taxonomy `PRESS/CLICK/DOUBLE/LONG`, UP/DOWN/OK on one ADC ladder. The simulator models the event taxonomy only; ADC voltages, noise, debounce, and wake timing are explicitly not simulated.
- Button/caption constants from firmware: `kPassportSubtitlePageMs = 2500`, `kPassportEmojiSize = 32`, `PASSPORT_SCREEN_RADIUS = 30`, 240x320 portrait.
- Text limits (initial, subject to heap measurement): user 512 UTF-8 bytes, assistant 2 KiB; truncate only at UTF-8 boundaries, show truncation explicitly.
- Test patterns to mirror: `tests/test_bsp_display_rounding.c` (row-span vs per-pixel equivalence over full frame, corners, radius transitions, clamped/degenerate radii, NULL/out-of-range rejection).
- Rendering references (read-only, do not copy wholesale): `bsp_display*.c`, `main/ui_pixel*.{h,c}`, `demo_display.c`.
- Process references: `skills/passport-develop/SKILL.md`, `docs/development/ai-guide.md`, `tools/validate.sh`.
- License/assets: confirm redistribution rights before copying fonts/artwork (per `docs/DEVELOPMENT.md` rule 8); decide repo license before public binaries.

## 3. Architecture (narrow P0 slice of docs/ARCHITECTURE.md)

- `app/`: SDL2 window (240x320 virtual coords; optional integer zoom, screenshots always 1:1), keyboard Up/Down/Enter + clickable virtual buttons, scenario controls, SIMULATED labels, PNG capture, event log. Stage 1 uses a stub window so logic lands first.
- `include/passport_sim/` + `src/`: scenario runner (versioned JSON load/validate, monotonic virtual clock, play/pause/step/reset, event log). All page timers and timeouts run on virtual time.
- `shared/`: `screen_rounding.c` (verbatim import), `passport_activity.h` (verbatim) + `device_state_shim.h`, new `caption_buffers` + `layout` modules (owned by simulator, reviewed against firmware behavior).
- Render backend (stage 2): LVGL v9.5.0 via FetchContent + SDL display/input; 30 px rounded mask from shared geometry; placeholder fonts/assets with visible warnings.
- Virtual services (P0): `ButtonInput` + `ConversationEvents` (STT/TTS/state) + minimal `SettingsStoreMock`. Transport/Audio/Battery/Power stay as interface stubs for P1.
- Firmware adapters: event mapping only, never linking board/ESP-IDF code. Record firmware SHA (`yangguo/xiaozhi-esp32`) and simulator SHA per release.

## 4. Components and ownership

| Unit | Owns | Interface | Depends on |
|---|---|---|---|
| glass_geometry (`shared/`) | safe rect, subtitle viewport, mask spans, page count/offset | `screen_rounding.h` (imported) | libc only |
| activity (`shared/`) | Idle/Listening/Thinking/Speaking transitions | `passport_activity.h` + shim | libc only |
| caption_buffers (`shared/`) | separate bounded user/assistant buffers, STT-retention rules, UTF-8-safe truncation | `caption_buffers.h` | libc only |
| layout (`shared/` or `src/`) | idle vs caption-priority viewport computation from resolution/radius/status reserve/line height | `layout.h` | glass_geometry |
| scenario_runner | JSON validation, scheduling on virtual clock, play/pause/step/reset, deterministic log | `scenario_runner.h` | virtual_clock, presentation |
| virtual_clock | monotonic ms clock, advance-to-next-event | `virtual_clock.h` | nothing |
| button_input (mock) | press/release/click/double/long synthesis from keyboard/mouse/scenario | `ButtonInput` | presentation |
| conversation_events (mock) | STT/TTS/error/activity injection, SIMULATED-labeled | `ConversationEvents` | presentation |
| sdl_shell (`app/`) | window, input, toolbar, screenshots, log view | main entry | LVGL, SDL2 (stage 2) |

Rules: valid STT replaces previous user utterance; TTS start/sentences/state changes/empty system messages never erase it; new TTS turn resets assistant buffer only; empty/invalid STT ignored; never draw outside glass safe area or over status/alert; 2.5 s page timer on virtual clock.

## 5. Data flow

1. SDL input or scenario event enters a virtual service mock.
2. Service emits a normalized event into the presentation/state module.
3. Module updates bounded caption/state data and asks LVGL to render (stage 1: state only, verified by unit/scenario tests; stage 2: real render).
4. Runner records event + resulting state with virtual timestamp.
5. Screenshot tests capture the 240x320 viewport at stable checkpoints.

## 6. Error handling

- Invalid scenario (bad JSON, unknown version/type, out-of-order time, oversize, invalid UTF-8): reject before playback with file path + event index + reason; no partial execution.
- Oversized text: enforce shared byte limits, log truncation/rejection at UTF-8 boundary.
- SDL/window failure: clear exit error; unit tests still run headless.
- Missing fonts/assets: documented placeholder + warning; never silently substitute copyrighted art.
- Scenario reset: restores clock, mocks, captions, page timers, state, log.

## 7. Testing and acceptance

- Unit (doctest, headless, no SDL window): geometry/mask, layout selection incl. menu/error, STT retention lifecycle, TTS-turn reset, UTF-8 truncation/limits, schema validation, key routing, settings transaction outcomes.
- Scenario (virtual clock, deterministic): the 10 starter scenarios in `docs/TEST_STRATEGY.md` (normal PTT, STT persistence, empty STT, long mixed-text pagination, Speaking interruption, disconnect/timeout, packet gap/reorder/queue-full, missing/low battery, menu open/close, dim/sleep/wake sequence).
- Screenshot regression: 240x320 viewport only; pin LVGL/fonts/SDL backend/color depth/OS image; small reviewed golden set per key state; CI publishes actual/expected/diff on mismatch; geometry assertions backstop pixel diffs.
- Gates: clean macOS + Ubuntu checkout builds/tests per documented commands; deterministic replay (identical log + screenshots); all UI inside safe area; mocks labeled SIMULATED; CI runs unit + scenario + screenshot jobs; shared-code changes also run the Passport firmware build/size gate separately; short real-device checklist records what simulation cannot verify.

## 8. Phased exit criteria

- Stage 1 (skeleton + portable core): CMake configures/builds/tests on clean checkout; doctest suite green; scenario runner replays fixtures deterministically headless; no LVGL dependency yet.
- Stage 2 (real rendering): LVGL v9.5.0 + SDL window at 240x320; both subtitle layouts render in safe area; golden screenshots for both layouts + key overlays; P0-01..P0-04 gates pass; >=10 scenarios in CI.

## 9. Open items for the implementation plan

- Resolved LVGL patch version from `dependencies.lock`; FetchContent URLs/SHAs.
- doctest version pin; PNG capture lib choice (e.g. stb_image_write vs SDL_image) and image-diff tool/threshold.
- Font/asset selection with license provenance.
- Repo license decision before public redistribution.
- Whether first configure may download deps or must work offline after bootstrap.
