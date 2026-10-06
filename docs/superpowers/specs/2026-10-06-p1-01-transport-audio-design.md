# P1-01 Transport + Audio-Queue Test Doubles (spec)

- Date: 2026-10-06
- Scope: P1-01 only (P1-02 settings, P1-03 battery/power, P1-04 QEMU are separate)
- Status: brainstormed and user-approved section by section; pending spec review, then implementation plan
- Approach: A — normalized service layer (rejected: seeded symptom scripts, display-only)

## 1. Decisions already made

- Symptoms drive UI state: disconnect/timeout clears Thinking-class activity and shows recovery prompts (not counters-only).
- Per-packet audio modeling: every packet advances seq/depth/drop/reorder counters, mirroring firmware `fixed_queue` behavior (not water-level-only).
- Protocol-agnostic transport layer: one channel state machine for both MQTT and WebSocket, modeled on firmware `Protocol` callbacks (`OnNetworkError`, `OnDisconnected`, `IsTimeout`). No sockets, no reconnect backoff, no RF claims.
- New first-class fixture events (`transport`, `audio_packet`) replace P0 `note` placeholders for these symptoms; `note` stays valid for everything else.
- Determinism carries over from P0: single-threaded virtual clock, no RNG, no wall-clock, byte-identical replay logs, golden screenshots.

## 2. Architecture

P0 layering is unchanged. P1-01 grows the `Virtual device services` layer only:

- **TransportMock** (`include/passport_sim/transport_mock.h`): channel state machine `Up → Degraded → Down`. Fixture `transport` events drive `link up|down`, `timeout <ms>`, `error "<msg>"`, `recover`. Outputs: `channel_open()` (feeds the existing `PassportResolveActivity` call, replacing P0's hardcoded "listening sets true, never closes"), `has_error()` (feeds the existing parameter P0 always passed as false), error string (feeds the alert overlay). `Degraded` is entered on timeout-before-deadline configs only where a fixture needs a warning phase; plain down/error go straight to Down.
- **AudioPipelineMock** (`include/passport_sim/audio_pipeline_mock.h`): four bounded queues (encode/send/decode/playback) mirroring the `audio_service.h` pipeline comment. Capacities configurable, defaults send/decode 32, playback 8, encode 32 (to confirm against firmware `fixed_queue` sizing when measured). `audio_packet` events advance one packet at a time: `{seq, queue}` enqueues, full queue drops with `dropped++` (mirrors `encode_drop_count`), explicit `reorder:true` marks reordered arrivals (counted only, never re-sorted), seq jumps counted as `gaps`. Packet bytes are never stored (deliberate YAGNI). Zero backpressure on TTS: full queues never block the scenario, only counters move.
- **Presentation changes (two places)**: `ScenarioRunner` gains `transport`/`audio_packet` branches, maintains `channel_open_` by asking TransportMock every step, routes `has_error=true` through `PassportResolveActivity` (Thinking suppressed), and sends the error string to the alert overlay. `LvglShell` reuses the verified alert position for transport errors/recovery and appends the queue waterline to the status row. SIMULATED labeling everywhere.
- **Fixtures**: rewrite `disconnect-timeout.json` and `packet-gap-reorder-queue-full.json` with the new events (delete the `note` versions, no dual tracks); add `transport-recovery.json` and `queue-saturation.json`; goldens grow by `transport-down.png`, `transport-recovered.png`, `queue-full.png`. P0's six goldens are re-run, diffs reviewed under the golden workflow.

## 3. Components and interfaces

```cpp
enum class LinkState { Up, Degraded, Down };
class TransportMock {
 public:
  void link_down(std::string_view reason);
  void link_up();
  void timeout(uint32_t after_ms);   // relative deadline from now_ms
  void error(std::string_view msg);  // Down + error string to alert
  void recover();                    // back Up, clears error string
  bool channel_open() const;         // false iff Down
  bool has_error() const;
  std::string_view error_message() const;
  LinkState state() const;
  bool deadline_due(uint32_t now_ms) const;
  void reset();
};
```

```cpp
enum class AudioQueue { Encode, Send, Decode, Playback };
struct QueueCounters {
  uint32_t depth = 0, capacity = 32, dropped = 0, reordered = 0, gaps = 0;
  uint32_t last_seq = 0;
  bool has_seq = false;
};
class AudioPipelineMock {
 public:
  explicit AudioPipelineMock(/* per-queue capacities, defaults above */);
  void packet(uint32_t seq, AudioQueue q);  // enqueue or drop+count
  void mark_reordered(uint32_t seq);        // count only
  void drain(AudioQueue q);                 // consumer drains (tts_stop path)
  const QueueCounters& counters(AudioQueue q) const;
  uint32_t total_dropped() const;
  std::string status_line() const;  // exact format pinned by unit test
  void reset();
};
```

Responsibilities: TransportMock is a pure channel state machine (no clock, no network); AudioPipelineMock is per-queue accounting (no packet bytes); ScenarioRunner translates the two new event types and owns the activity/error-string routing; LvglShell only displays. `note` stays valid. `status_line()` format, `LinkState` transitions, capacity defaults, and the relative-deadline semantic are all pinned by unit tests — no "reasonable defaults" left undocumented.

## 4. Data flow (reference trace: disconnect mid-Thinking, then recover)

1. `{"at_ms":1500,"type":"transport","action":"down","reason":"wifi lost"}` → clock advances, `transport_.link_down(...)`.
2. `channel_open()` false → `PassportResolveActivity(Thinking, Idle, false, true)` clears Thinking to None (firmware rule); error string `"wifi lost (SIMULATED)"` goes to the alert overlay.
3. Log line records link/error/activity; caption buffers untouched (disconnect never erases captions — extension of F-02).
4. `recover` returns the channel to Up and clears the alert; activity does NOT auto-return to Thinking — the next `state` event decides (matches firmware "reconnect doesn't restore the chip"; the simulator invents no behavior).
5. `audio_packet` events move counters only (`depth`, `gaps`, `reordered`, `dropped`); the log line carries `Q D32/32 ↓1`-style waterlines. Packets never touch clock, captions, or activity.
6. `tts_stop` drains Playback/Send queues to zero; `reset()` restores clock, captions, buttons, settings, transport, and audio to initial state (P0 reset semantics extended).
7. Screenshots render at `--at-ms` checkpoints only: alert present/gone, waterline visible.

Invariants (enforced by tests): packets affect counters only; transport affects channel/error and thereby activity, never captions directly.

## 5. Error handling

- Load-time strict validation with P0's error format (`event {i}: ...`): `transport` allows only `at_ms/type/action` plus `reason` (required non-empty string for down/error) or `after_ms` (required positive integer for timeout); `audio_packet` allows only `at_ms/type/queue/seq` plus optional `reorder:true`; `queue` must be one of the four, `seq` a non-negative integer (non-monotonic allowed — reorder is the point); `reason` follows P0 text rules (valid UTF-8, ≤8 KiB).
- `timeout.after_ms` is a RELATIVE deadline from the event's `at_ms`. The runner checks `deadline_due(clock.now_ms())` on every subsequent step and flips to Down with a synthetic log line (`[deadline] transport timeout expired -> link=Down`). Virtual time only moves forward, so deadlines can neither leak nor fire early.
- Runtime semantics: repeated `down` is idempotent, keeps the FIRST reason (`already down` in log); `recover` while Up is a no-op (`recover while Up`); draining an empty queue is a no-op; seq fallback (retransmit) enqueues normally. Mock methods are effectively non-throwing (one short error string, no failure paths).
- Host/render: waterline overflow truncates via existing LVGL wrap rules (string asserted in unit test, not pixels); golden threshold unchanged (0.1% / RGB distance 8); alert copy must pass a text assertion before its golden counts.
- `_bad_transport.json` joins the `_bad_*.json` family (unknown action, missing reason, illegal queue).

## 6. Testing and acceptance (P1-01 exit gate)

- Unit (headless doctest): `test_transport_mock.cc` (full transition table, idempotent down keeps first reason, no-op recover, deadline boundary incl. same-ms expiry, `channel_open/has_error` truth table); `test_audio_pipeline.cc` (depth accumulation, 32+5 → `dropped==5` with depth pinned, gap/reorder/retransmit counters independent, drain zeroes, `status_line()` exact match); `test_scenario_runner.cc` additions (P0's ten fixtures behave identically after the hardcoded channel logic moves into TransportMock — especially Thinking appearing in ptt-normal; new bad-fixture rejections; reset clears transport+audio).
- Scenario (deterministic): rewritten disconnect/packet-gap fixtures with `link=` and `Q` waterlines in log lines; new `transport-recovery.json` (down clears Thinking + alert on, recover clears alert, next `state` decides activity — "no auto-return" pinned); new `queue-saturation.json` (decode 32+5 → `dropped==5`, TTS still completes — "full queue never blocks captions" pinned); replay-all covers all twelve.
- Screenshot: three new goldens (`transport-down`, `transport-recovered`, `queue-full`) generated, eyeballed, checked in; P0 six re-run and diffs reviewed (expect only alert-related changes); one deliberate `recover`-as-noop perturbation must turn a golden red (gate-bite proof, P0 method).
- Acceptance: (1) all twelve fixtures green + byte-identical replay; (2) nine goldens green; (3) disconnect-clears-Thinking / recover-doesn't-restore / full-queue-doesn't-block-TTS each guarded by a fail-red test; (4) every counter/error string SIMULATED-labeled; firmware build gate stays separate (shared imports are read-only).

## 7. Open items for the implementation plan

- Exact queue capacities vs firmware `fixed_queue` sizing (defaults: send/decode 32, playback 8, encode 32; confirm or adjust with evidence).
- `Degraded` state triggers: which fixtures need a warning phase, if any (else implement Up/Down only and drop Degraded — YAGNI).
- `status_line()` exact format string.
- Whether `error` while Up implies Down (spec says yes) vs Degraded.
