#include <doctest/doctest.h>

#include <cstdint>
#include <string>

#include "caption_buffers.h"
#include "passport_sim/button_input.h"
#include "passport_sim/scenario_runner.h"
#include "passport_sim/settings_store.h"
#include "passport_sim/virtual_clock.h"

using passport_sim::AudioPipelineMock;
using passport_sim::AudioQueue;
using passport_sim::ButtonInput;
using passport_sim::CaptionBuffers;
using passport_sim::ScenarioRunner;
using passport_sim::SettingsStore;
using passport_sim::TransportMock;
using passport_sim::VirtualClock;

namespace {

struct Harness {
  VirtualClock clock;
  CaptionBuffers captions;
  ButtonInput buttons{clock};
  SettingsStore settings;
  TransportMock transport;
  AudioPipelineMock audio;
  ScenarioRunner runner{clock, captions, buttons, settings, transport, audio};
};

}  // namespace

TEST_CASE("runner rejects unknown version and backward time") {
  Harness h;
  auto bad_ver = h.runner.load("tests/scenarios/_bad_version.json");
  CHECK_FALSE(bad_ver.ok);
  auto bad_time = h.runner.load("tests/scenarios/_bad_time.json");
  CHECK_FALSE(bad_time.ok);
  CHECK(bad_time.error.find("event 2") != std::string::npos);
}

TEST_CASE("runner rejects unknown event type") {
  Harness h;
  auto bad = h.runner.load("tests/scenarios/_bad_type.json");
  CHECK_FALSE(bad.ok);
  CHECK(bad.error.find("unknown type") != std::string::npos);
}

TEST_CASE("failed load leaves runner empty and clean") {
  Harness h;
  auto bad = h.runner.load("tests/scenarios/_bad_time.json");
  CHECK_FALSE(bad.ok);
  CHECK_FALSE(h.runner.step());
  CHECK(h.runner.log().empty());
}

TEST_CASE("button mock synthesizes click, long, and double") {
  Harness h;
  h.buttons.press(passport_sim::Button::Ok);
  h.clock.advance_to(100);
  h.buttons.release(passport_sim::Button::Ok);
  CHECK(h.buttons.last_event() == passport_sim::ButtonEvent::Click);
  h.buttons.press(passport_sim::Button::Up);
  h.clock.advance_to(800);
  h.buttons.release(passport_sim::Button::Up);
  CHECK(h.buttons.last_event() == passport_sim::ButtonEvent::Long);
  h.clock.advance_to(900);
  h.buttons.press(passport_sim::Button::Down);
  h.clock.advance_to(950);
  h.buttons.release(passport_sim::Button::Down);
  h.clock.advance_to(1000);
  h.buttons.press(passport_sim::Button::Down);
  h.clock.advance_to(1050);
  h.buttons.release(passport_sim::Button::Down);
  CHECK(h.buttons.last_event() == passport_sim::ButtonEvent::Double);
  h.buttons.reset();
  CHECK(h.buttons.last_event() == passport_sim::ButtonEvent::None);
}

TEST_CASE("settings stub round-trips and resets") {
  Harness h;
  h.settings.set("brightness", "75");
  CHECK(h.settings.get("brightness") == "75");
  CHECK(h.settings.get("missing", "dflt") == "dflt");
  h.settings.reset();
  CHECK(h.settings.get("brightness", "dflt") == "dflt");
}

TEST_CASE("user STT persists through every assistant sentence via stepping") {
  Harness h;
  REQUIRE(h.runner.load("tests/scenarios/stt-retained-through-tts.json").ok);
  // Step to just after the third assistant sentence (event index 6).
  for (int i = 0; i < 7; ++i) REQUIRE(h.runner.step());
  CHECK(h.captions.user() == "第一句用户话");
  CHECK(h.captions.assistant().find("助手第三句") != std::string::npos);
  CHECK(h.runner.activity() == PassportActivity::kSpeaking);
  h.runner.run();
  CHECK(h.captions.user() == "第二句用户话替换");
}

TEST_CASE("replay is byte-for-byte deterministic") {
  Harness a;
  Harness b;
  REQUIRE(a.runner.load("tests/scenarios/stt-retained-through-tts.json").ok);
  REQUIRE(b.runner.load("tests/scenarios/stt-retained-through-tts.json").ok);
  a.runner.run();
  // Interleave b's run with pauses to prove virtual-time determinism.
  REQUIRE(b.runner.step());
  REQUIRE(b.runner.step());
  b.runner.run();
  CHECK(a.runner.log() == b.runner.log());
}

TEST_CASE("runner exposes next event time for transport controls") {
  Harness h;
  REQUIRE(h.runner.load("tests/scenarios/ptt-normal.json").ok);
  CHECK(h.runner.next_at_ms() == 0);
  REQUIRE(h.runner.step());
  CHECK(h.runner.next_at_ms() == 450);
  h.runner.run();
  CHECK(h.runner.next_at_ms() == UINT32_MAX);
}

TEST_CASE("all ten starter fixtures load and replay") {
  const char* names[] = {"ptt-normal",
                         "stt-retained-through-tts",
                         "empty-stt-ignored",
                         "long-mixed-text-paging",
                         "speaking-interruption",
                         "disconnect-timeout",
                         "packet-gap-reorder-queue-full",
                         "missing-battery-low-alert",
                         "menu-open-close",
                         "dim-sleep-wake",
                         "transport-recovery",
                         "queue-saturation"};
  for (const char* n : names) {
    Harness h;
    const std::string path =
        std::string("tests/scenarios/") + n + ".json";
    INFO("fixture: " << path);
    auto r = h.runner.load(path);
    REQUIRE(r.ok);
    h.runner.run();
    CHECK_FALSE(h.runner.log().empty());
  }
}

TEST_CASE("transport rejects bad action, missing reason, bad queue") {
  Harness h;
  auto a = h.runner.load("tests/scenarios/_bad_transport_action.json");
  CHECK_FALSE(a.ok);
  CHECK(a.error.find("unknown transport action") != std::string::npos);
  auto b = h.runner.load("tests/scenarios/_bad_transport_reason.json");
  CHECK_FALSE(b.ok);
  auto c = h.runner.load("tests/scenarios/_bad_transport_queue.json");
  CHECK_FALSE(c.ok);
  CHECK(c.error.find("unknown queue") != std::string::npos);
}

TEST_CASE("transport down clears thinking, recover does not restore it") {
  Harness h;
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

TEST_CASE("captions survive a transport outage") {
  Harness h;
  REQUIRE(h.runner.load("tests/scenarios/transport-recovery.json").ok);
  h.runner.run();
  CHECK(h.captions.user() == "那明天呢");
  CHECK(h.captions.assistant() == "明天晴。");
  CHECK(h.runner.log().find("link=Down") != std::string::npos);
  CHECK(h.runner.log().find("link=Up") != std::string::npos);
}

TEST_CASE("saturated send queue drops but TTS still completes") {
  Harness h;
  REQUIRE(h.runner.load("tests/scenarios/queue-saturation.json").ok);
  h.runner.run();
  CHECK(h.audio.counters(passport_sim::AudioQueue::Send).depth == 0);
  CHECK(h.audio.counters(passport_sim::AudioQueue::Send).dropped == 5);
  CHECK(h.captions.assistant() == "第一条。");
}

TEST_CASE("decode burst caps depth and counts drops") {
  Harness h;
  REQUIRE(
      h.runner.load("tests/scenarios/packet-gap-reorder-queue-full.json").ok);
  h.runner.run();
  const auto& c = h.audio.counters(passport_sim::AudioQueue::Decode);
  CHECK(c.depth == 20);
  CHECK(c.dropped == 8);
  CHECK(c.gaps == 3);
  CHECK(c.reordered == 1);
  CHECK(h.captions.assistant().find("第三条通知") != std::string::npos);
}
