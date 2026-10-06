# AI Passport Simulator P0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the P0 desktop simulator (CMake + SDL2 window + LVGL v9.5.0 rendering at 240x320, portable caption/state/scenario core, golden screenshot regression, Ubuntu CI) in two gated stages.

**Architecture:** Portable `shared/` core (imported firmware geometry/activity + owned caption/layout/scenario modules, all headless-testable) under a thin `app/` SDL shell; LVGL rendering added in Stage 2 against the same layout rects. Every task lands with tests and its own commit.

**Tech Stack:** C++17, CMake 3.28+, SDL `release-2.32.10`, LVGL `v9.5.0`, doctest `v2.5.3`, nlohmann/json `v3.12.0`, stb (`2c980bb`, image write/compare), Ninja, GitHub Actions (Ubuntu 24.04), macOS local dev.

## Global Constraints

- Viewport is 240x320 virtual pixels; screenshots are always 1:1 (zoom never affects capture).
- Glass radius is 30 px (`PASSPORT_SCREEN_RADIUS`); no text outside the safe area or over status/alert.
- User text limit 512 UTF-8 bytes, assistant text limit 2 KiB; truncate only at UTF-8 boundaries, show truncation explicitly.
- Valid STT replaces the previous user utterance; TTS/state/empty events never erase it; new TTS turn resets assistant buffer only; empty/invalid STT is ignored.
- Subtitle page timer is 2500 ms on the virtual clock, never wall-clock.
- Every mocked value is labeled SIMULATED in UI and logs; no serial/GPIO/mic/network access by default.
- Imported firmware files stay byte-identical to upstream (MIT, both sources); divergences are recorded, never silently edited.
- No `TODO`/`TBD`/placeholder steps; each task ends green (`ctest --output-on-failure`) plus its commit.

---

## File Structure

```text
CMakeLists.txt                    # root: options, FetchContent, ctest
cmake/FetchDeps.cmake             # all third-party pins in one place
app/main.cc                       # SDL shell: window, input, toolbar, screenshots
include/passport_sim/
  virtual_clock.h                 # monotonic ms clock
  scenario_runner.h               # fixture load/validate/replay/step/reset/log
  button_input.h                  # UP/DOWN/OK press/release/click/double/long
  conversation_events.h           # STT/TTS/state/error injection (SIMULATED)
  settings_store.h                # P0: in-memory settings stub w/ reset
shared/
  screen_rounding.h/.c            # VERBATIM from FoloToy/ai-passport bsp_display_rounding
  passport_activity.h             # VERBATIM from xiaozhi-esp32 ai-passport
  device_state_shim.h             # minimal DeviceState enum copy for host
  caption_buffers.h/.cc           # owned: user/assistant buffers + retention rules
  layout.h/.cc                    # owned: idle vs caption-priority rect computation
src/
  virtual_clock.cc
  scenario_runner.cc
  button_input.cc
  conversation_events.cc
  settings_store.cc
  screenshot_png.cc               # stb-based viewport PNG writer (Stage 2)
tests/unit/test_main.cc           # doctest main only
tests/unit/test_geometry.cc
tests/unit/test_activity.cc
tests/unit/test_caption_buffers.cc
tests/unit/test_layout.cc
tests/unit/test_scenario_runner.cc
tests/scenarios/*.json            # 10 fixtures (names in Task 6)
tests/golden/*.png                # reviewed goldens (Stage 2)
.github/workflows/ci.yml         # Ubuntu configure/build/test (+ screenshots Stage 2)
docs/DEVICE_CHECKLIST.md          # what simulation cannot verify (Task 10)
```

Conventions: headers own the contract (Doxygen one-liner per public function); `shared/` has zero SDL/LVGL/ESP-IDF includes; mocks live behind `include/passport_sim` interfaces only.

---

### Task 1: Toolchain bootstrap + CMake skeleton + doctest + CI

**Files:**
- Create: `cmake/FetchDeps.cmake`, `CMakeLists.txt`, `tests/unit/test_main.cc`, `.github/workflows/ci.yml`
- Modify: `README.md` (replace provisional commands with verified ones only after Stage 1 builds)

**Interfaces:**
- Consumes: none.
- Produces: `add_passport_test(name sources...)` CMake helper (used by Tasks 2-6); `ctest` suite runnable.

- [ ] **Step 1: Install toolchain (macOS)**

Run: `brew install cmake ninja pkg-config sdl2`
Expected: `cmake --version` prints 3.28+, `ninja --version` prints 1.11+.

- [ ] **Step 2: Write `cmake/FetchDeps.cmake` with exact pins**

```cmake
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.5.3)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0)
FetchContent_Declare(SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.32.10)
# Stage 2 additions (declare now, enable with PASSPORT_WITH_LVGL=ON):
#  LVGL v9.5.0, stb 2c980bb for stb_image_write.h
FetchContent_MakeAvailable(doctest nlohmann_json)
```

- [ ] **Step 3: Write root `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.28)
project(ai_passport_simulator LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PASSPORT_WITH_LVGL "Build real LVGL/SDL rendering (Stage 2)" OFF)
include(cmake/FetchDeps.cmake)
enable_testing()
add_subdirectory(tests/unit)
```

plus `tests/unit/CMakeLists.txt` with the helper:

```cmake
function(add_passport_test name)
  add_executable(${name} test_main.cc ${ARGN})
  target_link_libraries(${name} PRIVATE doctest::doctest)
  add_test(NAME ${name} COMMAND ${name})
endfunction()
```

- [ ] **Step 4: Write `tests/unit/test_main.cc`**

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
```

- [ ] **Step 5: Write `.github/workflows/ci.yml` (Ubuntu 24.04, configure + build + ctest)**

```yaml
name: host
on: [push, pull_request]
jobs:
  build-test:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get update && sudo apt-get install -y cmake ninja-build pkg-config libsdl2-dev
      - run: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
      - run: cmake --build build --parallel
      - run: ctest --test-dir build --output-on-failure
```

- [ ] **Step 6: Configure, build, test locally**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 7: Commit**

```bash
git add cmake CMakeLists.txt tests .github
git commit -m "feat: add CMake skeleton with doctest and Ubuntu CI"
```

---

### Task 2: Glass geometry import + unit tests

**Files:**
- Create: `shared/screen_rounding.h`, `shared/screen_rounding.c`, `tests/unit/test_geometry.cc`
- Modify: `tests/unit/CMakeLists.txt` (add `add_passport_test(test_geometry test_geometry.cc geometry_lib)` and a `geometry` static lib target for `shared/screen_rounding.c`)

**Interfaces:**
- Consumes: none.
- Produces: `passport_rounded_row_span`, `passport_glass_safe_rect`, `passport_subtitle_viewport`, `passport_rect_inside_glass`, `passport_subtitle_page_count`, `passport_subtitle_page_offset` (all from the imported header; later tasks call `passport_glass_safe_rect(240, 320, 30, &rect)` and `passport_subtitle_viewport(...)`).

- [ ] **Step 1: Copy files byte-identical and record provenance**

Run: `cp /Users/vyang/Desktop/spaces/xiaozhi-esp32/main/boards/folotoy/ai-passport/screen_rounding.h shared/screen_rounding.h && cp /Users/vyang/Desktop/spaces/xiaozhi-esp32/main/boards/folotoy/ai-passport/screen_rounding.c shared/screen_rounding.c && diff shared/screen_rounding.c /Users/vyang/Desktop/spaces/xiaozhi-esp32/main/boards/folotoy/ai-passport/screen_rounding.c && diff <(sed 's/passport_/bsp_display_/g' shared/screen_rounding.h) "$(gh api repos/FoloToy/ai-passport/contents/components/bsp/src/bsp_display_rounding.h --jq '.content' | base64 -d)" ; echo "upstream-diff-exit=$?"`
Expected: first diff silent; second diff shows only the `passport_` vs `bsp_display_` rename (record any other hunk in `docs/superpowers/specs/2026-10-06-ai-passport-simulator-p0-design.md` section 2 as a divergence). Add header comment to both files: `// Imported verbatim from FoloToy/ai-passport (MIT) via yangguo/xiaozhi-esp32; do not edit, record divergences in the P0 spec.`

- [ ] **Step 2: Write the failing test (mirror official `test_bsp_display_rounding.c`)**

```cpp
#include <doctest/doctest.h>
#include "screen_rounding.h"

TEST_CASE("rounded row span matches per-pixel mask at 240x320 r30") {
  const int W = 240, H = 320, R = 30;
  CHECK(passport_rect_inside_glass(&(passport_rect_t){30, 0, 1, 1}, W, H, R));
  CHECK_FALSE(passport_rect_inside_glass(&(passport_rect_t){0, 0, 1, 1}, W, H, R));
  for (int y = 0; y < H; ++y) {
    int32_t x1 = -1, x2 = -1;
    CHECK(passport_rounded_row_span(y, W, H, R, &x1, &x2));
  }
  int32_t x1 = 0, x2 = 0;
  CHECK_FALSE(passport_rounded_row_span(-1, W, H, R, &x1, &x2));
  CHECK_FALSE(passport_rounded_row_span(0, W, H, R, nullptr, &x2));
  passport_rect_t safe{};
  CHECK(passport_glass_safe_rect(W, H, R, &safe));
  CHECK(safe.width == W);
  CHECK(passport_rect_inside_glass(&safe, W, H, R));
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_geometry --output-on-failure`
Expected: FAIL (target `test_geometry` does not exist yet).

- [ ] **Step 4: Add build targets (geometry lib + test) then rerun**

In root `CMakeLists.txt` append:

```cmake
add_library(geometry STATIC shared/screen_rounding.c)
target_include_directories(geometry PUBLIC shared)
```

In `tests/unit/CMakeLists.txt` append: `add_passport_test(test_geometry test_geometry.cc)` and `target_link_libraries(test_geometry PRIVATE geometry)`.

Run: `cmake -S . -B build -G Ninja && cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 5: Commit**

```bash
git add shared tests CMakeLists.txt
git commit -m "feat: import passport glass geometry with unit tests"
```

---

### Task 3: Activity state + DeviceState shim + unit tests

**Files:**
- Create: `shared/passport_activity.h` (verbatim copy), `shared/device_state_shim.h`, `tests/unit/test_activity.cc`
- Modify: `tests/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: `shared/device_state_shim.h` provides `enum DeviceState { kDeviceStateUnknown, kDeviceStateStarting, kDeviceStateWifiConfiguring, kDeviceStateIdle, kDeviceStateConnecting, kDeviceStateListening, kDeviceStateSpeaking, kDeviceStateNotifying, kDeviceStateUpgrading, kDeviceStateActivating, kDeviceStateAudioTesting, kDeviceStateFatalError };` (value-for-value copy of firmware `main/device_state.h`).
- Produces: `PassportNextActivity(PassportActivity, DeviceState)`, `PassportResolveActivity(current, state, channel_open, has_error)`, `PassportThinkingClears(current, state, channel_open)`, `PassportActivityWakesScreen(activity, state)` for Task 5/6/7.

- [ ] **Step 1: Copy activity header and write the shim**

Run: `cp /Users/vyang/Desktop/spaces/xiaozhi-esp32/main/boards/folotoy/ai-passport/passport_activity.h shared/passport_activity.h`
Expected: exit 0. Then create `shared/device_state_shim.h`:

```cpp
// Host-only stand-in for firmware main/device_state.h (values must match).
#pragma once
enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateNotifying,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateAudioTesting,
    kDeviceStateFatalError
};
```

Edit `shared/passport_activity.h` include line `#include "device_state.h"` to `#include "device_state_shim.h"` (the single allowed adaptation; note it in the commit message).

- [ ] **Step 2: Write the failing test**

```cpp
#include <doctest/doctest.h>
#include "passport_activity.h"

TEST_CASE("listening->idle with open channel becomes thinking") {
  CHECK(PassportResolveActivity(PassportActivity::kListening,
        kDeviceStateIdle, true, false) == PassportActivity::kThinking);
  CHECK(PassportResolveActivity(PassportActivity::kListening,
        kDeviceStateIdle, false, false) == PassportActivity::kNone);
  CHECK(PassportResolveActivity(PassportActivity::kListening,
        kDeviceStateIdle, true, true) == PassportActivity::kNone);
  CHECK(PassportNextActivity(PassportActivity::kNone,
        kDeviceStateSpeaking) == PassportActivity::kSpeaking);
  CHECK(PassportThinkingClears(PassportActivity::kThinking,
        kDeviceStateIdle, false));
  CHECK(PassportActivityWakesScreen(PassportActivity::kSpeaking,
        kDeviceStateSpeaking));
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build --parallel && ./build/tests/unit/test_activity 2>&1 | head -5`
Expected: FAIL (binary does not exist).

- [ ] **Step 4: Register header-only test target and rerun**

Append to `tests/unit/CMakeLists.txt`:

```cmake
add_passport_test(test_activity test_activity.cc)
target_include_directories(test_activity PRIVATE ${CMAKE_SOURCE_DIR}/shared)
```

(Also add the same `target_include_directories` line for `test_geometry` if missing.) Rerun full build + `ctest`.
Expected: `100% tests passed`.

- [ ] **Step 5: Commit**

```bash
git add shared tests
git commit -m "feat: import passport activity logic with host shim and tests"
```

---

### Task 4: Caption buffers (STT retention + UTF-8-safe truncation)

**Files:**
- Create: `shared/caption_buffers.h`, `shared/caption_buffers.cc`, `tests/unit/test_caption_buffers.cc`
- Modify: root `CMakeLists.txt` (add `caption` static lib), `tests/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing (pure libc++).
- Produces (exact signatures used by Tasks 5-8):

```cpp
namespace passport_sim {
struct CaptionLimits { size_t user_max_bytes = 512; size_t assistant_max_bytes = 2048; };
class CaptionBuffers {
 public:
  explicit CaptionBuffers(CaptionLimits limits = {});
  // Returns false (keeps old text) for empty/invalid-UTF8 input.
  bool set_user_stt(std::string_view utf8);
  void begin_tts_turn();                       // clears assistant only
  void append_tts_sentence(std::string_view s); // appends with space separator
  void on_state_or_system_event();              // must not touch either buffer
  std::string_view user() const;
  std::string_view assistant() const;
  bool user_truncated() const;
  bool assistant_truncated() const;
  void reset();
 private:
  // truncate_at_utf8_boundary(), is_valid_utf8() helpers
};
}
```

- [ ] **Step 1: Write the failing lifecycle test**

```cpp
#include <doctest/doctest.h>
#include "caption_buffers.h"
using passport_sim::CaptionBuffers;

TEST_CASE("user STT survives TTS turn and state events") {
  CaptionBuffers b;
  CHECK(b.set_user_stt("明天下午天气怎么样？"));
  b.begin_tts_turn();
  b.append_tts_sentence("明天下午可能会下雨。");
  b.append_tts_sentence("记得带伞。");
  b.on_state_or_system_event();
  CHECK(b.user() == "明天下午天气怎么样？");
  CHECK(b.assistant() == "明天下午可能会下雨。 记得带伞。");
  CHECK_FALSE(b.set_user_stt(""));
  CHECK(b.user() == "明天下午天气怎么样？");
  b.begin_tts_turn();
  CHECK(b.assistant().empty());
  CHECK(b.user() == "明天下午天气怎么样？");
  CHECK(b.set_user_stt("那后天呢？"));
  CHECK(b.user() == "那后天呢？");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_caption --output-on-failure`
Expected: FAIL (target missing).

- [ ] **Step 3: Write minimal implementation**

`shared/caption_buffers.h` declares the interface above. `shared/caption_buffers.cc`:

```cpp
#include "caption_buffers.h"
namespace passport_sim {
namespace {
bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }
bool is_valid_utf8(std::string_view s) {
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t len = 1;
    if ((c & 0x80) == 0) len = 1;
    else if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    else return false;
    if (i + len > s.size()) return false;
    for (size_t k = 1; k < len; ++k)
      if (!is_continuation(static_cast<unsigned char>(s[i + k]))) return false;
    i += len;
  }
  return true;
}
std::string truncate_at_utf8_boundary(std::string_view s, size_t max_bytes) {
  if (s.size() <= max_bytes) return std::string(s);
  size_t n = max_bytes;
  while (n > 0 && is_continuation(static_cast<unsigned char>(s[n]))) --n;
  return std::string(s.substr(0, n));
}
}  // namespace

CaptionBuffers::CaptionBuffers(CaptionLimits limits) : limits_(limits) {}
bool CaptionBuffers::set_user_stt(std::string_view utf8) {
  if (utf8.empty() || !is_valid_utf8(utf8)) return false;
  user_ = truncate_at_utf8_boundary(utf8, limits_.user_max_bytes);
  user_truncated_ = utf8.size() > limits_.user_max_bytes;
  return true;
}
void CaptionBuffers::begin_tts_turn() { assistant_.clear(); assistant_truncated_ = false; }
void CaptionBuffers::append_tts_sentence(std::string_view s) {
  if (s.empty() || !is_valid_utf8(s)) return;
  std::string add = truncate_at_utf8_boundary(s, limits_.assistant_max_bytes);
  if (!assistant_.empty()) assistant_ += " ";
  assistant_ += add;
  if (assistant_.size() > limits_.assistant_max_bytes) {
    assistant_ = truncate_at_utf8_boundary(assistant_, limits_.assistant_max_bytes);
    assistant_truncated_ = true;
  }
}
void CaptionBuffers::on_state_or_system_event() {}
// ... accessors and reset() as declared
}
```

(private members: `CaptionLimits limits_; std::string user_, assistant_; bool user_truncated_ = false, assistant_truncated_ = false;`)

- [ ] **Step 4: Register targets and run**

Root `CMakeLists.txt`: `add_library(caption STATIC shared/caption_buffers.cc)` + `target_include_directories(caption PUBLIC shared)`. Test: `add_passport_test(test_caption_buffers test_caption_buffers.cc)` + `target_link_libraries(test_caption_buffers PRIVATE caption)`.
Run full build + `ctest --output-on-failure`. Expected: PASS.

- [ ] **Step 5: Add truncation edge tests (mixed CJK/Latin/emoji, exact-boundary split)**

Append to `test_caption_buffers.cc` a case with a 4-byte emoji straddling byte 512 proving no split (bytes before boundary form valid UTF-8, `user_truncated()` true). Run `ctest`. Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add shared tests CMakeLists.txt
git commit -m "feat: add caption buffers with STT retention and UTF-8 truncation"
```

---

### Task 5: Layout computation (idle vs caption-priority)

**Files:**
- Create: `shared/layout.h`, `shared/layout.cc`, `tests/unit/test_layout.cc`
- Modify: root `CMakeLists.txt` (add `layout` lib), `tests/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: `passport_glass_safe_rect`, `passport_subtitle_viewport`, `passport_activity_line`, `passport_widget_place_t` (Task 2); `PassportActivity` (Task 3).
- Produces (exact signatures used by Tasks 7-8):

```cpp
namespace passport_sim {
struct LayoutRects {
  passport_rect_t safe{};
  passport_rect_t subtitle_viewport{};
  passport_rect_t activity_line{};
  passport_widget_place_t subtitle_place{};
  bool caption_priority = false;  // true in Thinking/Speaking
};
class Layout {
 public:
  Layout(int width = 240, int height = 320, int radius = 30,
         int line_height = 16, int top_reserve = 20, int emoji_half = 16);
  // caption_priority == (activity is Thinking or Speaking)
  LayoutRects compute(PassportActivity activity) const;
 private:
  int width_, height_, radius_, line_height_, top_reserve_;
  int emoji_half_idle_, emoji_half_caption_;
};
}
```

Rule: idle/listening uses `emoji_half = 16` (32 px emoji); thinking/speaking uses `emoji_half = 0` (expression shrunk into top safe area) so the subtitle viewport grows. `compute` CHECK-fails (doctest `REQUIRE`) when `passport_subtitle_viewport` returns false.

- [ ] **Step 1: Write the failing test**

```cpp
#include <doctest/doctest.h>
#include "layout.h"
using passport_sim::Layout;

TEST_CASE("caption-priority viewport is taller than idle viewport") {
  Layout l;
  auto idle = l.compute(PassportActivity::kListening);
  auto speaking = l.compute(PassportActivity::kSpeaking);
  CHECK_FALSE(idle.caption_priority);
  CHECK(speaking.caption_priority);
  CHECK(speaking.subtitle_viewport.height > idle.subtitle_viewport.height);
  CHECK(idle.subtitle_viewport.y >= idle.safe.y);
  CHECK(speaking.subtitle_viewport.y >= speaking.safe.y);
  int bottom = speaking.subtitle_viewport.y + speaking.subtitle_viewport.height;
  CHECK(bottom <= speaking.safe.y + speaking.safe.height);
}
```

- [ ] **Step 2-4:** Same TDD cycle as Task 4 (fail on missing target, implement `layout.cc` calling the three geometry functions with the emoji_half rule, register `layout` lib linking `geometry`, full `ctest` green).
- [ ] **Step 5: Commit**

```bash
git add shared tests CMakeLists.txt
git commit -m "feat: add adaptive subtitle layout computation"
```

---

### Task 6: Virtual clock + scenario runner + 10 fixtures + headless CLI (Stage 1 gate)

**Files:**
- Create: `include/passport_sim/virtual_clock.h`, `src/virtual_clock.cc`, `include/passport_sim/scenario_runner.h`, `src/scenario_runner.cc`, `include/passport_sim/conversation_events.h`, `src/conversation_events.cc`, `include/passport_sim/button_input.h`, `src/button_input.cc`, `include/passport_sim/settings_store.h`, `src/settings_store.cc`, `tests/unit/test_scenario_runner.cc`, `tests/scenarios/*.json` (10 files), `app/replay.cc` (headless `--scenario X --log Y` CLI)
- Modify: root `CMakeLists.txt` (add `sim_core` lib + `passport-replay` binary), `tests/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: `CaptionBuffers`, `Layout`, activity functions, geometry (Tasks 2-5).
- Produces: `passport_sim::VirtualClock { void advance_to(uint32_t ms); uint32_t now_ms() const; void reset(); }`; `ScenarioRunner::{ LoadResult load(path); void reset(); bool step(); void run(); std::string log() const; }` where `LoadResult { bool ok; std::string error; }` with errors like `"tests/scenarios/foo.json event 3: 'at_ms' goes backwards (700 < 1200)"`; event types `state|stt|tts_start|tts_sentence|tts_stop|key|note` (unknown type rejected); `passport-replay --scenario <path> [--log <path>]` exit codes `0 ok, 2 bad usage, 3 invalid fixture`.

Fixture schema (exact): `{"scenario_version": 1, "name": "...", "events": [{"at_ms": N, "type": "...", ...}]}`; `at_ms` non-negative integers, monotonic (equal allowed, file order kept); UTF-8 strings bounded by caption limits.

The 10 fixtures (exact filenames): `ptt-normal.json`, `stt-retained-through-tts.json`, `empty-stt-ignored.json`, `long-mixed-text-paging.json`, `speaking-interruption.json`, `disconnect-timeout.json`, `packet-gap-reorder-queue-full.json`, `missing-battery-low-alert.json`, `menu-open-close.json`, `dim-sleep-wake.json`. Each carries `"simulated": true` in a top-level `"meta"` object. Battery/power/menu events in Stage 1 are recorded in the log as `note` entries (real mocks land in P1; the log line proves determinism, nothing claims hardware fidelity).

- [ ] **Step 1: Write the failing runner validation test**

```cpp
#include <doctest/doctest.h>
#include "passport_sim/scenario_runner.h"
TEST_CASE("runner rejects unknown version and backward time") {
  passport_sim::ScenarioRunner r;
  auto bad_ver = r.load("tests/scenarios/_bad_version.json");
  CHECK_FALSE(bad_ver.ok);
  auto bad_time = r.load("tests/scenarios/_bad_time.json");
  CHECK_FALSE(bad_time.ok);
  CHECK(bad_time.error.find("event 2") != std::string::npos);
}
```

(with the two `_bad_*.json` fixtures created alongside).

- [ ] **Step 2: Run to verify it fails** (`ctest -R test_scenario` → target missing).
- [ ] **Step 3: Implement `virtual_clock`, `scenario_runner` (nlohmann/json parsing, bounds: max 4096 events, max 8 KiB per text field), the three mock classes as state-recording stubs, and `sim_core` lib.**
- [ ] **Step 4: Register targets, run green.** Then write the 10 fixtures + a determinism test replaying `stt-retained-through-tts.json` twice and comparing logs byte-for-byte, plus a retention test asserting the final log contains the user text after `tts_stop`.
- [ ] **Step 5: Build `passport-replay` CLI** (`app/replay.cc`): parses `--scenario/--log`, prints the event log to stdout or file, implements exit codes 0/2/3; add a CTest invoking it on `ptt-normal.json` and diffing stdout against `tests/scenarios/ptt-normal.expected.log` (checked in).
- [ ] **Step 6: Verify Stage 1 gate**: `cmake -S . -B build -G Ninja && cmake --build build --parallel && ctest --output-on-failure` green; `./build/passport-replay --scenario tests/scenarios/stt-retained-through-tts.json` prints deterministic log.
- [ ] **Step 7: Commit**

```bash
git add include src app tests CMakeLists.txt
git commit -m "feat: add scenario runner, mocks, fixtures, and headless replay"
```

---

### Task 7: SDL shell with input wiring (placeholder rendering from layout rects)

**Files:**
- Create: `app/main.cc`
- Modify: root `CMakeLists.txt` (add `passport-simulator` binary linking SDL2 + `sim_core` + `layout`), fixture `menu-open-close.json` if key handling needs new event fields (no schema change: `key` events already carry `{"key": "up|down|ok", "action": "press|release"}`)

**Interfaces:**
- Consumes: `ButtonInput::press/release` from keyboard Up/Down/Return and clickable on-screen buttons; `Layout::compute`; long-press via virtual-clock hold threshold 600 ms (constant `kLongPressMs = 600` in `button_input.h`).
- Produces: window titled `"AI Passport Simulator — SIMULATED"`; status line always shows `SIMULATED`; placeholder rendering draws layout rects as colored SDL outlines (safe=grey, subtitle=green, activity=blue) — real pixels arrive in Task 8.

- [ ] **Step 1: Write failing input-routing test** (headless, in `tests/unit/test_button_input.cc`): press+release within 600 ms yields `CLICK`; hold past 600 ms on virtual clock yields `LONG`; double press yields `DOUBLE`. Run → missing target.
- [ ] **Step 2: Implement `button_input.cc`** with the `ButtonInput` class driven by `VirtualClock`.
- [ ] **Step 3: Implement `app/main.cc`**: 240x320 SDL window (integer zoom x2 via `SDL_RenderSetLogicalSize`), event loop mapping keys/mouse to `ButtonInput`, scenario transport controls (space=play/pause, n=step, r=reset), rect-outline rendering from `Layout::compute`, `S`=save state-log screenshot placeholder (real PNG in Task 8).
- [ ] **Step 4: Manual verify**: `./build/passport-simulator` opens, Up/Down/Return drive menu highlight in `menu-open-close.json` replay; window title contains SIMULATED.
- [ ] **Step 5: Commit**

```bash
git add app include src tests CMakeLists.txt
git commit -m "feat: add SDL shell with virtual button input"
```

---

### Task 8: LVGL v9.5.0 bring-up + both layouts + PNG screenshots (Stage 2)

**Files:**
- Create: `src/screenshot_png.cc`, `src/lvgl_shell.cc` (+ `src/lvgl_shell.h`), `tests/unit/test_screenshot.cc`
- Modify: `cmake/FetchDeps.cmake` (add LVGL + stb), root `CMakeLists.txt` (wire `PASSPORT_WITH_LVGL=ON`), `app/main.cc` (render via LVGL when enabled), `app/replay.cc` (`--screenshot <path>` flag)

**Interfaces:**
- Consumes: `LayoutRects` from Task 5; `CaptionBuffers` contents; `passport_mask_rgb565_area` for the flush mask.
- Produces: `SaveViewportPng(path, rgb565, w, h)` (stb_image_write, returns bool); `LvglShell::render(state)` drawing status row, expression placeholder, `我:` user caption, `AI:` assistant caption, alert region; `passport-replay --scenario X --screenshot out.png` writes the 240x320 PNG headless.

Pins added to `FetchDeps.cmake`:

```cmake
FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.5.0)
FetchContent_Declare(stb  GIT_REPOSITORY https://github.com/nothings/stb.git GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20)
```

LVGL config: `lv_conf.h` with `LV_MEM_SIZE (64*1024)`, `LV_USE_SDL` off (host drives flush via `lv_display` + `SDL_Texture` update), color depth 16 (RGB565 to match firmware flush path).

- [ ] **Step 1: Extend FetchDeps, configure with LVGL on** — expect configure to succeed; build will fail (no LVGL sources referenced yet).
- [ ] **Step 2: Write failing screenshot test**: render known 2x2 RGB565 buffer, `SaveViewportPng` to temp path, assert file exists and re-decodes to identical pixels (via `stb_image.h`).
- [ ] **Step 3: Implement `screenshot_png.cc`** (stb_image_write `stbi_write_png` from converted RGB888 rows).
- [ ] **Step 4: Implement `lvgl_shell`**: screen 240x320, status label (top, `passport_status_bar_place`), expression box (32x32 idle center / shrunk top in caption-priority), subtitle label inside `subtitle_place` rect with `passport_subtitle_label_y(page_offset)` paging on the 2500 ms virtual timer, `我:`/`AI:` prefixes, activity chip hidden when `PassportActivityDuplicatesStatus` (avoids the double-聆听中 bug), low-battery/error overlay from `activity_line` rect, SIMULATED watermark.
- [ ] **Step 5: Wire `--screenshot`** into `passport-replay` (headless LVGL render at final event + optional `--at-ms N` checkpoint flag).
- [ ] **Step 6: Verify both layouts visually**: `--scenario ptt-normal.json --screenshot` at listening and speaking checkpoints; assert nothing drawn outside `passport_rect_inside_glass` (geometry assertion in test, not just eyeballing).
- [ ] **Step 7: Commit**

```bash
git add cmake src app tests CMakeLists.txt
git commit -m "feat: bring up LVGL rendering with dual subtitle layouts"
```

---

### Task 9: Golden screenshots + CI regression job

**Files:**
- Create: `tests/golden/*.png` (initial set: `idle.png`, `listening.png`, `thinking.png`, `speaking-long.png`, `menu.png`, `low-battery.png`), `tools/compare_png.cc` (pixel-diff with threshold, writes diff PNG), `tests/golden/CMakeLists.txt` (or CTest script entries)
- Modify: `.github/workflows/ci.yml` (add screenshot job + artifact upload), `docs/DEVELOPMENT.md` (document golden update/review steps)

**Interfaces:**
- Consumes: `passport-replay --scenario --screenshot --at-ms` (Task 8), `SaveViewportPng`.
- Produces: `compare-png <expected> <actual> <diff-out>` exit 0 identical / 1 different (threshold: max 0.1% pixels differ, each within RGB distance 8 — exact constants in `tools/compare_png.cc`); CI fails with actual/expected/diff artifacts on mismatch.

- [ ] **Step 1: Generate initial goldens** from reviewed renders (`./build/passport-replay --scenario tests/scenarios/ptt-normal.json --screenshot ...` per checkpoint) and eyeball-review each against the layout rects.
- [ ] **Step 2: Write compare tool + failing test** (identical files → 0; one-pixel flip → 1 with diff PNG written).
- [ ] **Step 3: Register golden CTests** (one per golden: replay fixture → compare) and extend CI with the screenshot job + `actions/upload-artifact` for `actual/expected/diff` on failure.
- [ ] **Step 4: Prove the gate works**: deliberately shift `top_reserve` by 2 px, watch CI-case fail locally with a useful diff, revert.
- [ ] **Step 5: Commit**

```bash
git add tests tools .github docs
git commit -m "feat: add golden screenshot regression and CI artifacts"
```

---

### Task 10: Docs, device checklist, release 0.1 gate

**Files:**
- Create: `docs/DEVICE_CHECKLIST.md`
- Modify: `README.md` (verified commands only), `docs/DEVELOPMENT.md` (actual commands + golden workflow), spec section 2 (final firmware SHA + LVGL resolved version)

**Interfaces:**
- Consumes: everything above.
- Produces: `docs/DEVICE_CHECKLIST.md` listing every REQUIREMENTS non-goal as an unchecked real-device item (audio quality, ADC/debounce, RF path, battery measurements, heap pressure, glass optics, full-firmware boot) with columns: item, why simulation can't cover it, how to verify on device.

- [ ] **Step 1: Record provenance**: `git -C /Users/vyang/Desktop/spaces/xiaozhi-esp32 rev-parse HEAD` and `git -C /Users/vyang/Desktop/spaces/FoloToy 2>/dev/null || gh api repos/FoloToy/ai-passport/commits/main --jq '.sha'`; write both SHAs + LVGL `v9.5.0 (85aa60d)` into the spec and README.
- [ ] **Step 2: Verify clean-checkout flow** on this Mac from scratch (`rm -rf build && cmake ... && ctest`), paste exact output into README; confirm Ubuntu via CI run link.
- [ ] **Step 3: Walk the Release 0.1 exit checklist** (ROADMAP: P0-01..P0-04 gates, ≥10 scenarios in CI, docs distinguish limits, license decision recorded or explicitly deferred with owner+date).
- [ ] **Step 4: Commit**

```bash
git add docs README.md
git commit -m "docs: verify commands and add device checklist for 0.1"
```

---

## Self-Review

**1. Spec coverage:** §3 architecture → Tasks 1/7/8; §4 components table → Tasks 2-7 (each row has a task); §5 data flow → Tasks 6-8; §6 error handling → Task 6 validation + Task 8 SDL-failure path (headless tests always run: unit tests never link SDL/LVGL — enforced by `shared/` and `sim_core` having no SDL includes); §7 testing → Tasks 2-6 unit, Task 6 ten scenarios, Task 9 goldens, Task 10 firmware/device gates; §8 exit criteria → Task 6 (Stage 1) + Task 10 (Stage 2); §9 open items → resolved inline (LVGL v9.5.0, doctest v2.5.3, json v3.12.0, stb 2c980bb, PNG via stb, fonts deferred to Task 8 as documented placeholders).

**2. Placeholder scan:** no TBD/TODO; every version pinned to a verified tag/SHA; thresholds numeric (600 ms long-press, 2500 ms page, 0.1%/RGB-8 diff, 4096 events, 8 KiB text field); error strings exemplified.

**3. Type consistency:** `CaptionBuffers`, `Layout`/`LayoutRects`, `VirtualClock`, `ScenarioRunner`/`LoadResult`, `ButtonInput`, `kLongPressMs`, `SaveViewportPng`, event-type strings and fixture field names are spelled identically in every task that uses them. `shared/` functions keep their firmware C signatures (called from C++ via the imported headers).

One deliberate deviation from the spec draft: `conversation_events.h`/`button_input.h`/`settings_store.h` move to Stage 1 Task 6 as state-recording stubs (not P1) because the ten P0 scenario fixtures need their event vocabulary; P1 only adds real transport/audio/battery/power symptom modeling.
