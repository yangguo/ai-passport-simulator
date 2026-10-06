#include <doctest/doctest.h>

#include <string>

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

TEST_CASE("over-limit text truncates at UTF-8 boundary and flags it") {
  CaptionBuffers b;
  // 130 repetitions of U+660E (3 bytes each) = 390 bytes, then a 4-byte emoji
  // straddling the 512-byte limit: 170*3=510 bytes + emoji bytes 511..514.
  std::string s;
  for (int i = 0; i < 170; ++i) s += "明";
  s += "🌧";
  s += "尾";
  REQUIRE(s.size() > 512);
  CHECK(b.set_user_stt(s));
  CHECK(b.user_truncated());
  CHECK(b.user().size() <= 512);
  // Still valid UTF-8: re-setting the stored value must succeed.
  CaptionBuffers c;
  CHECK(c.set_user_stt(b.user()));
  CHECK(c.user() == b.user());
  // The emoji straddling the boundary was cut, the tail char is gone.
  CHECK(b.user().find("尾") == std::string::npos);
}

TEST_CASE("invalid UTF-8 and empty assistant input are ignored") {
  CaptionBuffers b;
  CHECK(b.set_user_stt("有效文本"));
  CHECK_FALSE(b.set_user_stt("\xff\xfe invalid"));
  CHECK(b.user() == "有效文本");
  b.begin_tts_turn();
  b.append_tts_sentence("");
  b.append_tts_sentence("\x80" "bad");
  CHECK(b.assistant().empty());
  b.reset();
  CHECK(b.user().empty());
  CHECK(b.assistant().empty());
  CHECK_FALSE(b.user_truncated());
}
