# P1-01 Transport + Audio-Queue Test Doubles Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Promote P0's transport/audio `note` placeholders to state-driving mocks with per-packet accounting, new fixture events, and three new goldens.

**Architecture:** Two new narrow mocks (`TransportMock`, `AudioPipelineMock`) behind `include/passport_sim` interfaces; `ScenarioRunner` translates the two new event types and routes channel/error into the existing activity logic; `LvglShell` reuses the verified alert overlay and appends a waterline to the status row. No threads, no RNG, virtual clock only.

**Tech Stack:** C++17, CMake/Ninja, doctest (existing), no new dependencies.

## Global Constraints

- Deterministic: same fixture replays byte-identical logs and screenshots.
- `note` events stay valid; only the two rewritten fixtures change vocabulary.
- Packets affect counters only — never clock, captions, or activity.
- Transport affects channel/error and thereby activity, never captions directly.
- Every mocked value labeled SIMULATED in UI and logs.
- Imported firmware files stay byte-identical; queue capacities below are measured values from `audio_service.h`, recorded with evidence, not guesses.
- No `TODO`/`TBD`/placeholder steps; each task ends green plus its commit.

---

## File Structure

```text
include/passport_sim/
  transport_mock.h      # NEW: LinkState {Up, Down}, channel + error string
  audio_pipeline_mock.h # NEW: 4 bounded queues, per-packet counters
src/
  transport_mock.cc     # NEW
  audio_pipeline_mock.cc# NEW
  scenario_runner.cc    # MODIFY: 2 new event branches, channel/error routing
include/passport_sim/scenario_runner.h  # MODIFY: accessors, ScenarioEvent fields
src/lvgl_shell.cc       # MODIFY: alert text + status waterline
tests/unit/
  test_transport_mock.cc# NEW
  test_audio_pipeline.cc# NEW
  test_scenario_runner.cc # MODIFY: append coverage
tests/scenarios/
  disconnect-timeout.json               # REWRITE with transport events
  packet-gap-reorder-queue-full.json    # REWRITE with audio_packet events
  transport-recovery.json               # NEW
  queue-saturation.json                 # NEW
  _bad_transport.json                   # NEW (3 rejection cases)
tests/golden/
  transport-down.png transport-recovered.png queue-full.png  # NEW
```

Conventions (P0, unchanged): headers own contracts; `shared/` untouched;
mocks behind `include/passport_sim` only; error strings reuse P0 format
`event {i}: ...` with file path prefix.

## Resolved open items (from spec §7, decided with evidence before planning)

- **Capacities** (measured from firmware `main/audio/audio_service.h`):
  encode tasks 2 (`MAX_ENCODE_TASKS_IN_QUEUE`), send packets 40
  (`2400/OPUS_FRAME_DURATION_MS`, 60 ms frames), decode packets 20
  (`1200/60`), playback tasks 2. `FixedQueue::push_back` returns false when
  full — drops are counted, never block (matches `encode_drop_count`).
- **Degraded dropped** (YAGNI): no fixture needs a warning phase;
  `LinkState { Up, Down }` only. If a later milestone needs it, it becomes
  its own spec.
- **status_line() exact format**: `"s 31/40 d 12/20 drop 3"` — send and
  decode are the firmware's "main queues"; total drops across all four
  queues; pure ASCII (Montserrat has no arrows/symbols for this).
- **error while Up implies Down**: yes (spec interface comment is normative).

---

### Task 1: TransportMock channel state machine

**Files:**
- Create: `include/passport_sim/transport_mock.h`, `src/transport_mock.cc`
- Create: `tests/unit/test_transport_mock.cc`
- Modify: root `CMakeLists.txt` (add `transport` static lib + include dirs), `tests/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces (exact signatures used by Task 3):

```cpp
namespace passport_sim {
enum class LinkState { Up, Down };
class TransportMock {
 public:
  TransportMock() = default;
  void link_down(std::string_view reason);
  void link_up();
  void timeout(uint32_t after_ms);  // relative deadline from supplied now
  void error(std::string_view msg);  // Down + error string
  void recover();                    // Up, clears error
  bool channel_open() const;         // false iff Down
  bool has_error() const;
  std::string_view error_message() const;
  LinkState state() const;
  // Called by the runner every step with the virtual clock value.
  // Returns true once when a pending deadline expires (then clears it).
  bool poll_deadline(uint32_t now_ms);
  void reset();
 private:
  LinkState state_ = LinkState::Up;
  std::string error_;
  bool deadline_armed_ = false;
  uint32_t deadline_at_ms_ = 0;
};
}
```

`timeout(after_ms)` arms `deadline_at_ms_ = <caller's now> + after_ms` — but
the mock owns no clock, so the signature is `timeout(uint32_t now_ms,
uint32_t after_ms)`. (The spec sketch omitted the now parameter; the plan
is authoritative: two parameters.) `poll_deadline` flips to Down with a
generic error `"timeout"` only if still Up when the deadline passes; a
`recover`/`link_up` before expiry disarms. Repeated `down` keeps the FIRST
reason. `recover` while Up is a no-op. `link_up` clears error and disarms
but is otherwise identical to `recover` (both exist because fixtures read
better that way: `up` after manual down, `recover` after timeout).

- [ ] **Step 1: Write the failing test**

```cpp
#include <doctest/doctest.h>
#include "passport_sim/transport_mock.h"
using passport_sim::LinkState;
using passport_sim::TransportMock;

TEST_CASE("link down/up/recover cycle") {
  TransportMock t;
  CHECK(t.channel_open());
  CHECK_FALSE(t.has_error());
  t.link_down("wifi lost");
  CHECK_FALSE(t.channel_open());
  CHECK(t.has_error());
  CHECK(t.error_message() == "wifi lost");
  t.link_down("second reason");
  CHECK(t.error_message() == "wifi lost");  // first reason kept
  t.recover();
  CHECK(t.channel_open());
  CHECK(t.error_message().empty());
  t.recover();  // no-op while Up
  CHECK(t.channel_open());
}

TEST_CASE("timeout deadline fires once, disarmed by recover") {
  TransportMock t;
  t.timeout(1000, 500);
  CHECK_FALSE(t.poll_deadline(1200));
  CHECK(t.channel_open());
  CHECK(t.poll_deadline(1500));
  CHECK_FALSE(t.channel_open());
  CHECK_FALSE(t.poll_deadline(1600));  // fires once
  t.reset();
  CHECK(t.channel_open());
  t.timeout(0, 100);
  t.recover();
  CHECK_FALSE(t.poll_deadline(500));  // disarmed
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_transport --output-on-failure`
Expected: FAIL (target `test_transport_mock` does not exist).

- [ ] **Step 3: Write minimal implementation**

`src/transport_mock.cc`:

```cpp
#include "passport_sim/transport_mock.h"

namespace passport_sim {

void TransportMock::link_down(std::string_view reason) {
  if (state_ == LinkState::Down) return;  // keep first reason
  state_ = LinkState::Down;
  error_ = std::string(reason);
}

void TransportMock::link_up() {
  state_ = LinkState::Up;
  error_.clear();
  deadline_armed_ = false;
}

void TransportMock::timeout(uint32_t now_ms, uint32_t after_ms) {
  deadline_armed_ = true;
  deadline_at_ms_ = now_ms + after_ms;
}

void TransportMock::error(std::string_view msg) {
  state_ = LinkState::Down;
  error_ = std::string(msg);
}

void TransportMock::recover() { link_up(); }

bool TransportMock::channel_open() const {
  return state_ != LinkState::Down;
}

bool TransportMock::has_error() const { return !error_.empty(); }

std::string_view TransportMock::error_message() const { return error_; }

LinkState TransportMock::state() const { return state_; }

bool TransportMock::poll_deadline(uint32_t now_ms) {
  if (!deadline_armed_ || now_ms < deadline_at_ms_) return false;
  deadline_armed_ = false;
  if (state_ == LinkState::Up) {
    state_ = LinkState::Down;
    error_ = "timeout";
    return true;
  }
  return false;
}

void TransportMock::reset() {
  state_ = LinkState::Up;
  error_.clear();
  deadline_armed_ = false;
  deadline_at_ms_ = 0;
}

}  // namespace passport_sim
```

Header needs `<string>` + `<string_view>` includes and the same member list.

- [ ] **Step 4: Register targets and run green**

Root `CMakeLists.txt`:

```cmake
add_library(transport STATIC src/transport_mock.cc)
target_include_directories(transport PUBLIC include)
```

`tests/unit/CMakeLists.txt`:

```cmake
add_passport_test(test_transport_mock test_transport_mock.cc)
target_link_libraries(test_transport_mock PRIVATE transport)
```

Run: `cmake -S . -B build -G Ninja && cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: all tests PASS (16 total after this task... state actual count).

- [ ] **Step 5: Commit**

```bash
git add include/passport_sim/transport_mock.h src/transport_mock.cc tests/unit/test_transport_mock.cc CMakeLists.txt tests/unit/CMakeLists.txt
git commit -m "feat: add TransportMock channel state machine"
```

---

### Task 2: AudioPipelineMock per-packet accounting

**Files:**
- Create: `include/passport_sim/audio_pipeline_mock.h`, `src/audio_pipeline_mock.cc`
- Create: `tests/unit/test_audio_pipeline.cc`
- Modify: root `CMakeLists.txt` (add `audio_mock` lib), `tests/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces (exact signatures used by Task 3):

```cpp
namespace passport_sim {
enum class AudioQueue { Encode, Send, Decode, Playback };
struct QueueCapacities {
  uint32_t encode = 2;
  uint32_t send = 40;
  uint32_t decode = 20;
  uint32_t playback = 2;
};
struct QueueCounters {
  uint32_t depth = 0;
  uint32_t capacity = 0;
  uint32_t dropped = 0;
  uint32_t reordered = 0;
  uint32_t gaps = 0;
  uint32_t last_seq = 0;
  bool has_seq = false;
};
class AudioPipelineMock {
 public:
  explicit AudioPipelineMock(QueueCapacities caps = {});
  // Enqueue one packet; full queue drops it and counts (never blocks).
  void packet(uint32_t seq, AudioQueue q);
  void mark_reordered(uint32_t seq);  // counts only; supply seq for log use
  void drain(AudioQueue q);
  const QueueCounters& counters(AudioQueue q) const;
  uint32_t total_dropped() const;
  // Exact: "s <send-depth>/<send-cap> d <dec-depth>/<dec-cap> drop <total>"
  std::string status_line() const;
  void reset();
 private:
  QueueCapacities caps_;
  QueueCounters per_queue_[4];
};
}
```

`packet` rules: if `depth == capacity` → `dropped++`, still record `last_seq`
and gap check (a dropped packet still advances the stream position); else
`depth++`. Gap check on every packet with a previous seq: if `seq >
last_seq + 1` → `gaps++` (covers jumps; equal-or-older seq = retransmit, no
gap, still enqueues-or-drops normally). `mark_reordered` increments
`reordered` on the Decode queue counters (reorder is a decode-side
phenomenon; the `seq` parameter is accepted for future log detail and
currently unused — mark `[[maybe_unused]]`). `drain` zeroes `depth` only
(counters persist for the session so post-mortem logs keep totals; `reset`
clears everything).

- [ ] **Step 1: Write the failing test**

```cpp
#include <doctest/doctest.h>
#include "passport_sim/audio_pipeline_mock.h"
using passport_sim::AudioPipelineMock;
using passport_sim::AudioQueue;

TEST_CASE("depth accumulates and drain zeroes") {
  AudioPipelineMock a;
  a.packet(41, AudioQueue::Decode);
  a.packet(42, AudioQueue::Decode);
  CHECK(a.counters(AudioQueue::Decode).depth == 2);
  CHECK(a.counters(AudioQueue::Decode).gaps == 0);
  a.drain(AudioQueue::Decode);
  CHECK(a.counters(AudioQueue::Decode).depth == 0);
}

TEST_CASE("full queue drops and counts, stream position still advances") {
  AudioPipelineMock a;
  for (uint32_t s = 0; s < 40; ++s) a.packet(s, AudioQueue::Send);
  CHECK(a.counters(AudioQueue::Send).depth == 40);
  for (uint32_t s = 40; s < 45; ++s) a.packet(s, AudioQueue::Send);
  CHECK(a.counters(AudioQueue::Send).depth == 40);
  CHECK(a.counters(AudioQueue::Send).dropped == 5);
  CHECK(a.total_dropped() == 5);
  CHECK(a.counters(AudioQueue::Send).last_seq == 44);
}

TEST_CASE("gap, reorder, and retransmit counters are independent") {
  AudioPipelineMock a;
  a.packet(41, AudioQueue::Decode);
  a.packet(44, AudioQueue::Decode);  // jump 42..43
  CHECK(a.counters(AudioQueue::Decode).gaps == 1);
  a.mark_reordered(43);
  CHECK(a.counters(AudioQueue::Decode).reordered == 1);
  CHECK(a.counters(AudioQueue::Decode).gaps == 1);
  a.packet(43, AudioQueue::Decode);  // late/retransmit: no new gap
  CHECK(a.counters(AudioQueue::Decode).gaps == 1);
  CHECK(a.counters(AudioQueue::Decode).depth == 3);
}

TEST_CASE("status line format is exact") {
  AudioPipelineMock a;
  for (uint32_t s = 0; s < 31; ++s) a.packet(s, AudioQueue::Send);
  for (uint32_t s = 0; s < 12; ++s) a.packet(s, AudioQueue::Decode);
  for (uint32_t s = 40; s < 43; ++s) a.packet(s, AudioQueue::Send);
  CHECK(a.status_line() == "s 31/40 d 12/20 drop 3");
}
```

Wait — check the last case arithmetic: send gets seq 0..30 (31 packets, depth 31, no gaps since first packet has no previous... first packet sets has_seq, no gap) then seq 40,41,42: 40 > 30+1 → gaps++ (1 gap); depth 31+3=34. Decode 0..11 → depth 12. status: send depth 34?? I wrote 31/40 — WRONG. Fix the test: after the extra 3, depth is 34. Correct expectation: `"s 34/40 d 12/20 drop 0"` — but then drop isn't exercised in the format. Rework: fill send to 40 first (seq 0..39), then 3 more (40..42, dropped=3, depth stays 40): status `"s 40/40 d 12/20 drop 3"`. Use that (and gaps from 0..39 sequential = 0; 40,41,42 sequential after 39 = no new gaps ✓).

- [ ] **Step 2: Run test to verify it fails** (same pattern: target missing).
- [ ] **Step 3: Write minimal implementation** (`src/audio_pipeline_mock.cc` per the rules above; `status_line()` builds `"s " + depth + "/" + cap + " d " + ... + " drop " + total` with `std::to_string`).
- [ ] **Step 4: Register targets, full `ctest` green.**
- [ ] **Step 5: Commit**

```bash
git add include/passport_sim/audio_pipeline_mock.h src/audio_pipeline_mock.cc tests/unit/test_audio_pipeline.cc CMakeLists.txt tests/unit/CMakeLists.txt
git commit -m "feat: add AudioPipelineMock per-packet accounting"
```

---

### Task 3: Runner event branches, validation, channel/error routing

**Files:**
- Modify: `include/passport_sim/scenario_runner.h`, `src/scenario_runner.cc`
- Create: `tests/scenarios/_bad_transport.json`
- Modify: `tests/unit/test_scenario_runner.cc` (append coverage)
- Modify: root `CMakeLists.txt` (`sim_core` gains both mock libs)

**Interfaces:**
- Consumes: `TransportMock::link_down/link_up/timeout/error/recover/poll_deadline/channel_open/has_error/error_message/state/reset`, `AudioPipelineMock::packet/mark_reordered/drain/counters/total_dropped/status_line/reset` (Tasks 1-2).
- Produces: `ScenarioRunner::transport()` and `::audio()` const accessors (used by Task 5 shell/LVGL reads and tests); extended `ScenarioEvent` with `transport_action`, `reason`, `after_ms`, `audio_queue`, `seq`, `reorder` fields; unchanged `load/step/run/reset/log` signatures.

New fixture vocabulary (strict whitelist, P0 error format):

```json
{"at_ms": 1500, "type": "transport", "action": "down", "reason": "wifi lost"}
{"at_ms": 1500, "type": "transport", "action": "up"}
{"at_ms": 1500, "type": "transport", "action": "timeout", "after_ms": 3000}
{"at_ms": 1500, "type": "transport", "action": "error", "reason": "mqtt refused"}
{"at_ms": 1500, "type": "transport", "action": "recover"}
{"at_ms": 1200, "type": "audio_packet", "queue": "decode", "seq": 44}
{"at_ms": 1200, "type": "audio_packet", "queue": "send", "seq": 45, "reorder": true}
```

Validation rules: `transport` allows exactly `at_ms/type/action` plus
`reason` (required non-empty valid-UTF-8 ≤8 KiB string for `down`/`error`)
or `after_ms` (required positive integer for `timeout`); unknown `action`
rejected. `audio_packet` allows exactly `at_ms/type/queue/seq` plus
optional `reorder` (must be boolean when present); `queue` in
`encode|send|decode|playback`; `seq` non-negative integer (non-monotonic
allowed). `timeout.after_ms` also accepts `0`? No — must be `>= 1`
(zero would fire on the same step; reject with `'after_ms' must be >= 1`).

Routing (in `step()`, after `clock.advance_to`):

```cpp
} else if (ev.type == "transport") {
  if (ev.transport_action == "down") transport_.link_down(ev.reason);
  else if (ev.transport_action == "up") transport_.link_up();
  else if (ev.transport_action == "timeout")
    transport_.timeout(clock_.now_ms(), ev.after_ms);
  else if (ev.transport_action == "error") transport_.error(ev.reason);
  else if (ev.transport_action == "recover") transport_.recover();
  activity_ = PassportResolveActivity(activity_, DeviceStateFor(activity_),
                                      transport_.channel_open(), transport_.has_error());
  ...
```

Hmm — `PassportResolveActivity` needs a `DeviceState`, but transport events
carry no state. Resolution: keep a `last_device_state_` member, updated on
every `state` event (default `kDeviceStateIdle`); transport steps recompute
`activity_ = PassportResolveActivity(activity_, last_device_state_,
transport_.channel_open(), transport_.has_error())`. This is exact and
minimal. AND: at the TOP of every `step()` (before applying the event),
check `transport_.poll_deadline(clock.now_ms())`; if it fired, recompute
activity the same way and append the synthetic log line
`[<now>] transport timeout expired -> activity=X link=Down`. Then apply the
current event normally.

Wait — ordering subtlety: deadline fires "on every subsequent step". If the
step's own event is at the same ms as the deadline, the timeout wins first
(deadline `<= now`), then the event applies. Pin this order in a unit test
(same-ms boundary case from Task 1's mock test, now at runner level).

Alert text: when `transport_.has_error()` is true after a transport step,
the shell (Task 5) reads `runner.transport().error_message()`; the LOG line
embeds `err="<msg>"` so headless assertions don't need LVGL. Runner itself
does not own widgets.

`tts_stop` additionally calls `audio_.drain(Playback)` and
`audio_.drain(Send)`. `reset()` additionally resets both mocks (log header
behavior unchanged).

Log line shapes (new, exact):

```text
[1500] transport down "wifi lost" -> activity=None link=Down err="wifi lost" user="..." assistant="..."
[4500] transport recover -> activity=None link=Up user="..." assistant="..."
[1200] audio_packet decode seq=44 -> depth=13 gaps=0 user="..." assistant="..."
```

`_bad_transport.json` (three cases in ONE file? No — load fails on the
FIRST error, so one file proves one rejection. Create three files:
`_bad_transport_action.json`, `_bad_transport_reason.json`,
`_bad_transport_queue.json`):

```json
{"scenario_version": 1, "name": "bad-transport-action",
 "events": [{"at_ms": 0, "type": "transport", "action": "explode"}]}
```

```json
{"scenario_version": 1, "name": "bad-transport-reason",
 "events": [{"at_ms": 0, "type": "transport", "action": "down"}]}
```

```json
{"scenario_version": 1, "name": "bad-transport-queue",
 "events": [{"at_ms": 0, "type": "audio_packet", "queue": "subwoofer", "seq": 1}]}
```

Test additions in `test_scenario_runner.cc`:

```cpp
TEST_CASE("transport down clears thinking, recover does not restore it") {
  // inline fixture via temp file? No — use tests/scenarios/transport-recovery.json (Task 4)?? 
```

Ordering problem: Task 3 tests need fixtures from Task 4. Resolution: Task 3
writes its OWN minimal inline fixtures as temp files? P0 pattern uses
`tests/scenarios/` files. Cleaner: Task 3 creates `tests/scenarios/transport-recovery.json`
(fixture only, no golden yet) and Task 4 reuses it + adds `queue-saturation.json`
+ rewrites. But Task 4 is "fixtures" task... Redefine boundaries: Task 3 =
runner + validation + `_bad_transport*.json` + a `transport-smoke.json`
micro-fixture for the routing test; Task 4 = the four real fixtures +
replay-all updates. The routing test then steps `transport-smoke.json`:

```json
{
  "scenario_version": 1, "name": "transport-smoke",
  "meta": {"simulated": true},
  "events": [
    {"at_ms": 0, "type": "state", "value": "listening"},
    {"at_ms": 300, "type": "state", "value": "thinking"},
    {"at_ms": 600, "type": "transport", "action": "down", "reason": "wifi lost"},
    {"at_ms": 900, "type": "transport", "action": "recover"},
    {"at_ms": 1200, "type": "state", "value": "listening"}
  ]
}
```

```cpp
TEST_CASE("transport down clears thinking, recover does not restore it") {
  Harness h;  // needs TransportMock/AudioPipelineMock members added to Harness
  REQUIRE(h.runner.load("tests/scenarios/transport-smoke.json").ok);
  REQUIRE(h.runner.step());  // listening
  REQUIRE(h.runner.step());  // thinking
  CHECK(h.runner.activity() == PassportActivity::kThinking);
  REQUIRE(h.runner.step());  // down
  CHECK(h.runner.activity() == PassportActivity::kNone);
  CHECK(h.runner.transport().error_message() == "wifi lost");
  REQUIRE(h.runner.step());  // recover
  CHECK(h.runner.activity() == PassportActivity::kNone);  // NOT restored
  CHECK(h.runner.transport().channel_open());
  REQUIRE(h.runner.step());  // listening again
  CHECK(h.runner.activity() == PassportActivity::kListening);
}
```

Harness gains `TransportMock transport; AudioPipelineMock audio;` and the
runner constructor takes both (constructor signature change:
`ScenarioRunner(clock, captions, buttons, settings, transport, audio)` —
update `app/replay.cc` construction + existing Harness accordingly; compiler
errors guide every call site, then run full suite).

Also: P0's ten fixtures must behave IDENTICALLY after the hardcoded channel
logic moves into TransportMock. The existing determinism test + replay-all
+ `ptt-normal.expected.log` CTest prove this with zero fixture changes: if
`state listening` no longer sets channel open, Thinking disappears and the
expected log fails. Implementation: `state` branch maps value→DeviceState as
before, sets `last_device_state_`, sets channel open on `listening`
(via `transport_.link_up()`? NO — link_up clears errors! Use a dedicated
method? Hmm. P0 semantic: listening opens the channel without touching
error state. If link_up clears errors, a `state listening` after a transport
error would silently clear it — WRONG. Add `TransportMock::note_channel_open()`
that sets Up WITHOUT clearing the error? That complicates the mock...

Cleaner: keep a `channel_open_` bool in the RUNNER (as P0), but DERIVE it:
`state listening` → `channel_open_ = true`; transport events set
`channel_open_ = transport_.channel_open()` after applying. I.e., the runner
keeps its own flag, transport mock is the authority only for transport
events. has_error: runner keeps `has_error_` from `transport_.has_error()`
after transport steps; state steps pass current `has_error_` through
(unchanged by state events). Reset clears both. This preserves P0 exactly
and adds transport cleanly — no new mock methods, no error-clearing hazard.
`transport()`/`audio()` accessors expose the mocks for UI/tests. ADOPT THIS
(runner-owned flags, mocks as event appliers + queryables).

So `step()` state branch:

```cpp
DeviceState dev = ToDeviceState(ev.value);
if (ev.value == "thinking") { channel_open_ = true; dev = kDeviceStateIdle; }
else if (ev.value == "listening") { channel_open_ = true; }
last_device_state_ = dev;
activity_ = PassportResolveActivity(activity_, dev, channel_open_, has_error_);
events_.system_event();
```

transport branch:

```cpp
// apply action to transport_ ...
if (transport_.poll_deadline(...)) // no — deadline check happens at step top
channel_open_ = transport_.channel_open();
has_error_ = transport_.has_error();
activity_ = PassportResolveActivity(activity_, last_device_state_, channel_open_, has_error_);
```

Step-top deadline check (before clock advance? after?): check with the
event's `at_ms` BEFORE advancing? Deadline semantics "expires when now
passes deadline": evaluate at step top using the INCOMING event time:
peek `events_[cursor_].at_ms`, `if (transport_.poll_deadline(next_at))` →
advance clock to... hmm, clock hasn't advanced yet. Synthetic log line needs
a timestamp: use `max(clock.now_ms(), deadline_time)`? The mock doesn't
expose deadline time... Add `uint32_t deadline_at_ms() const` accessor?
Or simpler: advance clock to the event time FIRST, then poll deadline with
now, then apply event:

```cpp
bool ScenarioRunner::step() {
  if (cursor_ >= events_.size()) return false;
  const ScenarioEvent ev = events_[cursor_++];
  clock_.advance_to(ev.at_ms);
  if (transport_.poll_deadline(clock_.now_ms())) {
    channel_open_ = transport_.channel_open();
    has_error_ = transport_.has_error();
    activity_ = PassportResolveActivity(activity_, last_device_state_, channel_open_, has_error_);
    log synthetic line with [clock.now_ms()];
  }
  ... apply ev as before ...
}
```

Same-ms order: deadline (evaluated at event time) wins over the event —
pinned by test. `deadline_due` vs `poll_deadline`: only `poll_deadline`
needed (it both checks and fires). Drop `deadline_due` from Task 1's
interface? Task 1 already specifies both... `deadline_due(now)` as a
non-consuming query is still useful for the shell's transport display; keep
both (query + poll). Fine.

`has_error_`/`channel_open_`/`last_device_state_` members init:
`channel_open_=false, has_error_=false, last_device_state_=kDeviceStateIdle`.
`reset()` restores all + both mocks + log cleared (existing).

- [ ] Steps follow TDD: validation test first (bad files), fail on missing
  branch (unknown type rejection already exists — new types FAIL LOADING
  until implemented, which doubles as the red step: write a GOOD fixture
  first? The red step: `transport-smoke.json` load must SUCCEED post-impl
  but the `transport` type is rejected pre-impl → test asserting `load ok`
  fails red naturally. Then implement branches. Then routing test.
  Then full suite green (P0 ten unchanged).
- [ ] Commit: `feat: route transport/audio events through new mocks`

---

### Task 4: Fixtures — rewrite two, add two

**Files:**
- Rewrite: `tests/scenarios/disconnect-timeout.json`, `tests/scenarios/packet-gap-reorder-queue-full.json`
- Create: `tests/scenarios/transport-recovery.json`, `tests/scenarios/queue-saturation.json`
- Modify: `tests/unit/test_scenario_runner.cc` (replay-all array +2, new assertions)

**Interfaces:** Consumes Task 3 vocabulary. Produces nothing new (data only).

`disconnect-timeout.json` (rewritten):

```json
{
  "scenario_version": 1,
  "name": "disconnect-timeout",
  "meta": {"simulated": true},
  "events": [
    {"at_ms": 0, "type": "state", "value": "listening"},
    {"at_ms": 300, "type": "stt", "text": "现在几点了"},
    {"at_ms": 600, "type": "state", "value": "thinking"},
    {"at_ms": 1500, "type": "transport", "action": "down", "reason": "wifi lost"},
    {"at_ms": 4500, "type": "transport", "action": "timeout", "after_ms": 3000},
    {"at_ms": 4600, "type": "state", "value": "idle"}
  ]
}
```

Wait — timeout AFTER down: arming a deadline while Down; `poll_deadline`
fires but state already Down → returns false, no synthetic line (my Task 1
impl returns false when not Up — good, no spurious line). Hmm, but then the
timeout event is a no-op observably... For fixture clarity use timeout
STANDALONE instead: replace the down with a timeout that expires mid-gap:

```json
    {"at_ms": 1500, "type": "transport", "action": "timeout", "after_ms": 3000},
    {"at_ms": 4600, "type": "state", "value": "idle"}
```

Trace: at 1500 arm deadline 4500; at 4600: advance, poll(4600) → fires
(Up→Down, error "timeout"), synthetic line `[4600] transport timeout
expired -> activity=None link=Down`, then state idle applies (already None).
Log shows the expiry. GOOD — exercises the synthetic path. Keep BOTH down
and timeout? One fixture, one mechanism per fixture (clear tests): this
fixture = timeout expiry. The down/recover path is covered by
transport-recovery.json. FINAL:

```json
{
  "scenario_version": 1, "name": "disconnect-timeout",
  "meta": {"simulated": true},
  "events": [
    {"at_ms": 0, "type": "state", "value": "listening"},
    {"at_ms": 300, "type": "stt", "text": "现在几点了"},
    {"at_ms": 600, "type": "state", "value": "thinking"},
    {"at_ms": 1500, "type": "transport", "action": "timeout", "after_ms": 3000},
    {"at_ms": 4600, "type": "state", "value": "idle"}
  ]
}
```

`packet-gap-reorder-queue-full.json` (rewritten, decode cap 20 — keep counts
under cap except the queue-full part which must EXCEED it; send cap 40):

```json
{
  "scenario_version": 1, "name": "packet-gap-reorder-queue-full",
  "meta": {"simulated": true},
  "events": [
    {"at_ms": 0, "type": "state", "value": "listening"},
    {"at_ms": 300, "type": "stt", "text": "播放通知"},
    {"at_ms": 600, "type": "state", "value": "thinking"},
    {"at_ms": 900, "type": "tts_start"},
    {"at_ms": 1100, "type": "audio_packet", "queue": "decode", "seq": 41},
    {"at_ms": 1150, "type": "audio_packet", "queue": "decode", "seq": 44},
    {"at_ms": 1200, "type": "tts_sentence", "text": "第一条通知。"},
    {"at_ms": 2000, "type": "audio_packet", "queue": "decode", "seq": 46, "reorder": true},
    {"at_ms": 2100, "type": "tts_sentence", "text": "第二条通知。"},
    {"at_ms": 2600, "type": "tts_sentence", "text": "第三条通知。"},
    {"at_ms": 3400, "type": "tts_stop"}
  ]
}
```

Hmm — this lost the queue-full part (name promises it). Queue-full needs
20+ decode packets — verbose but explicit is fine (fixtures are data):
append seq 100..124 (25 packets) + assert. That bloats the file; better:
dedicate `queue-saturation.json` to saturation (send queue 40+5) and keep
this fixture to gap/reorder with modest counts, RENAMED?
Renaming breaks the "ten starter fixtures" list... The P0 test array names
files; Task 4 updates the array. Cleaner: keep filename (stability for
docs referencing it), adjust NAME field? Filename↔name mismatch is
confusing. DECISION: keep `packet-gap-reorder-queue-full.json` with all
three phenomena (gap + reorder + a 25-packet saturation burst on decode:
seq 200..224 → depth caps at 20, dropped=5). Verbose is honest. And
`queue-saturation.json` focuses on the SEND queue (40+5) with TTS
completing (the no-backpressure proof). Both assert drops; different queues.

`transport-recovery.json`:

```json
{
  "scenario_version": 1, "name": "transport-recovery",
  "meta": {"simulated": true},
  "events": [
    {"at_ms": 0, "type": "state", "value": "listening"},
    {"at_ms": 300, "type": "stt", "text": "天气怎么样"},
    {"at_ms": 600, "type": "state", "value": "thinking"},
    {"at_ms": 1500, "type": "transport", "action": "down", "reason": "wifi lost"},
    {"at_ms": 4500, "type": "transport", "action": "recover"},
    {"at_ms": 5000, "type": "state", "value": "listening"},
    {"at_ms": 5300, "type": "stt", "text": "那明天呢"},
    {"at_ms": 5600, "type": "state", "value": "thinking"},
    {"at_ms": 5900, "type": "tts_start"},
    {"at_ms": 6100, "type": "tts_sentence", "text": "明天晴。"},
    {"at_ms": 7000, "type": "tts_stop"}
  ]
}
```

`queue-saturation.json`: listening/stt/thinking/tts_start, then send seq
0..44 (45 packets → depth 40, dropped 5), one sentence, tts_stop
(drains send+playback), idle. Assert final `total_dropped()==5` and
assistant text complete.

Tests: replay-all array +2 (twelve total); new assertions:
`transport-recovery` final user text == "那明天呢" (proves captions survived
the outage); `queue-saturation` drain leaves send depth 0 with dropped==5.

- [ ] Steps: write fixtures → extend replay-all (red: unknown types until
  Task 3? NO — Task 3 lands first, so Task 4 tests go green incrementally;
  red step here: new assertions on new behavior fail until... they're new
  code paths already implemented. The TDD red for Task 4 is the replay-all
  load of rewritten fixtures FAILING before rewrite? Circular. Honest
  framing: Task 4 is data; its "red" is running the suite after rewriting
  the two old fixtures but before updating expectations — do it in that
  order and show the failure (log-line format changed), then update tests.
- [ ] Commit: `feat: rewrite/add P1-01 scenario fixtures`

---

### Task 5: Alert text + status waterline in shell and screenshots

**Files:**
- Modify: `src/lvgl_shell.cc`, `include/passport_sim/lvgl_shell.h`
- Modify: `app/main.cc`, `app/replay.cc`
- Modify: `tests/unit/test_screenshot.cc` (append)

**Interfaces:** Consumes `runner.transport().error_message()/has_error()`,
`runner.audio().status_line()`, `LvglShell::set_alert/clear_alert`
(existing P0 API — reuse, no new methods).

Render rule (in BOTH `main.cc` frame loop and replay screenshot path —
extract a shared helper to avoid divergence; where? A tiny
`src/render_state.cc`? YAGNI — the logic is 6 lines, but two copies DO
diverge (P0 lesson). Put it on the runner? No — runner owns no widgets.
DECISION: add `LvglShell::render_scenario(const ScenarioRunner&)`? That
couples lvgl to runner (runner already depends on nothing LVGL — layering
violation inverted but harmless? runner is sim_core, lvglui already links
sim_core... `render_scenario` in lvglui taking `const ScenarioRunner&`
creates lvglui→sim_core dep — EXISTS already? lvglui links layout+caption
only. app links both. Adding lvglui→sim_core is a NEW edge. Alternative:
free function in app/ shared by main.cc and replay.cc:
`app/render_state.h` with `RenderScenario(LvglShell&, const
ScenarioRunner&)` — app-local, no new lib edges. ADOPT app-local helper.)

```cpp
// app/render_state.h
#pragma once
#include "passport_sim/lvgl_shell.h"
#include "passport_sim/scenario_runner.h"
namespace passport_sim {
// One place that maps runner state to widgets: captions+activity,
// transport error to alert overlay, audio waterline to status suffix.
inline void RenderScenario(LvglShell& shell, const ScenarioRunner& runner) {
  shell.render(runner.captions(), runner.activity());
  if (runner.transport().has_error())
    shell.set_alert(runner.transport().error_message().data());
  else
    shell.clear_alert();
  shell.set_status_suffix(runner.audio().status_line().c_str());
}
}
```

PROBLEM: `runner.captions()` accessor doesn't exist; `set_status_suffix`
doesn't exist on LvglShell. Add both (small, specified here):
- `ScenarioRunner::captions()` const accessor (Task 3 should have added;
  if missed, add here with test).
- `LvglShell::set_status_suffix(const char*)`: appends `" | " + suffix` to
  the status label text (empty suffix → plain status). Status label width:
  full 240, wrap off, truncate? Status text is short (`"Thinking | s
  40/40 d 12/20 drop 5"` ≈ 38 chars — too long for one 14px row (~30
  CJK-widths... ASCII ~8px → 300px > 240!). OVERFLOW. Resolve: waterline
  goes on its OWN second status row? No spare geometry... OR shorten:
  send/decode only when non-zero? Still long. DECISION: status label keeps
  activity text; waterline renders as a SECOND small label under the status
  row, inside the top of the subtitle viewport? That eats caption space...

Rethink: the activity_line rect (0,50,240,16) is EMPTY unless alert shows.
Waterline lives there when no alert; alert preempts it when error.
`set_status_suffix` positions a small label at activity_line when alert
hidden, hides it when alert shows. Geometry already unit-tested (Task P0-5
activity_line test). Label text exact `status_line()` string. When
`status_line()` is all-zero (`"s 0/40 d 0/20 drop 0"`)? Still show (proves
the plumbing in every golden) — yes, always show; it's SIMULATED-labeled by
the SIM watermark + log context... hmm, "every counter labeled SIMULATED":
the waterline itself can't fit "(SIMULATED)". The SIM watermark bottom-right
+ log header cover the labeling requirement (same rationale as P0 captions:
user text isn't individually tagged either). Document this decision in the
commit message.

So `LvglShell` gains: `set_status_suffix(const char*)` (positions at
activity_line, hidden iff alert visible — manage via internal flag).
`set_alert` hides the waterline label; `clear_alert` reshows it. Unit test
in test_screenshot.cc: set suffix → framebuffer has non-bg pixels in the
activity_line band; set alert → waterline pixels gone, alert pixels present
(compare band pixel counts, not exact images).

`main.cc` frame loop + replay screenshot path both call
`RenderScenario(shell, runner)` (replace direct `shell.render` calls;
replay keeps its `tick(100)` after).

- [ ] TDD: waterline test first (fails: no method), implement label +
  preemption, green. Then swap both call sites, full suite green (goldens:
  P0 six REGENERATE? Waterline label adds pixels to every screenshot →
  all six P0 goldens change! Expected: regenerate + review (new pixels only
  in activity_line band). Do it in this task, review carefully.)
- [ ] Commit: `feat: show transport alerts and audio waterline in UI`

---

### Task 6: Three goldens, gate proof, docs

**Files:**
- Create: `tests/golden/transport-down.png`, `tests/golden/transport-recovered.png`, `tests/golden/queue-full.png`
- Modify: root `CMakeLists.txt` (3 `add_golden_test` entries)
- Modify: `docs/DEVELOPMENT.md` (no doc change needed? golden workflow
  already documented — just follow it), `docs/DEVICE_EVIDENCE.md`? No.
  `shared/PROVENANCE.md`? No new imports. None.

Golden definitions:
- `transport-down`: `transport-recovery.json --at-ms 1500` (alert visible).
- `transport-recovered`: `transport-recovery.json --at-ms 4500` (alert gone,
  captions retained).
- `queue-full`: `queue-saturation.json` at final (waterline `drop 5` visible,
  assistant complete).

- [ ] Step 1: Generate via replay commands, eyeball-review each (alert
  present/gone, waterline exact string, captions retained, mask intact).
- [ ] Step 2: Register `add_golden_test` entries, full `ctest` green (now
  12 goldens? No — 9 total: 6 P0 + 3 new).
- [ ] Step 3: Gate-bite proof: temporarily make `recover()` a no-op
  (comment out state change), rebuild, show `transport-recovered` golden
  RED with diff artifact, revert, green.
- [ ] Step 4: P1-01 exit-gate walk (spec §6): twelve fixtures green +
  byte-determinism; nine goldens; three fail-red guards; SIMULATED labels;
  provenance (queue capacities cited to audio_service.h lines).
- [ ] Step 5: Commit: `feat: add P1-01 golden regression coverage`

---

## Self-Review

**1. Spec coverage:** §2 TransportMock → Task 1; AudioPipelineMock → Task 2
(capacities measured, not guessed); runner branches/validation/routing →
Task 3 (incl. `timeout` relative deadline, synthetic expiry line,
idempotent down, no-op recover); fixtures §2/§4 → Task 4 (rewrite 2, add
2, replay-all 12); UI §2/§5 → Task 5 (alert reuse, waterline,
SIMULATED rationale); testing §6 → Tasks 1-2 unit, 3-4 scenario,
5-6 screenshot; open items §7 → all resolved pre-plan (capacities measured,
Degraded dropped with reason, format pinned, error→Down normative).
`Degraded` enum value: spec §3 sketch still shows it — plan Task 1
implements `{Up, Down}` only; spec stays valid (conditional "where needed").

**2. Placeholder scan:** no TBD/TODO/"appropriate handling"; every error
string, format, capacity, threshold, and exit behavior is literal in the
plan. One honest data-task note in Task 4 (red-step ordering) — kept
because it prescribes an exact sequence, not a vague instruction.

**3. Type consistency:** `TransportMock::{link_down,link_up,timeout(now,after),error,recover,poll_deadline,channel_open,has_error,error_message,state,reset}`,
`AudioPipelineMock::{packet,mark_reordered,drain,counters,total_dropped,status_line,reset}`,
`QueueCounters::{depth,capacity,dropped,reordered,gaps,last_seq,has_seq}`,
`LinkState::{Up,Down}`, `AudioQueue::{Encode,Send,Decode,Playback}`,
`RenderScenario(shell, runner)`, `set_status_suffix`, `captions()` accessor
spelled identically everywhere. `ScenarioEvent` gains
`transport_action/reason/after_ms/audio_queue/seq/reorder` (Task 3).
`deadline_due` mentioned once in Task 3 narrative as kept query — Task 1
interface includes it. ✓
