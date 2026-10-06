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

TEST_CASE("thinking matches speaking, idle matches listening geometry") {
  Layout l;
  auto thinking = l.compute(PassportActivity::kThinking);
  auto speaking = l.compute(PassportActivity::kSpeaking);
  CHECK(thinking.caption_priority);
  CHECK(thinking.subtitle_viewport.height == speaking.subtitle_viewport.height);
  auto idle = l.compute(PassportActivity::kNone);
  auto listening = l.compute(PassportActivity::kListening);
  CHECK_FALSE(idle.caption_priority);
  CHECK(idle.subtitle_viewport.height == listening.subtitle_viewport.height);
  // Activity line sits below the status reserve and above the subtitle.
  CHECK(idle.activity_line.y >= idle.safe.y);
  CHECK(idle.activity_line.y + idle.activity_line.height <=
        idle.subtitle_viewport.y);
  // Subtitle placement is a non-scrollable top-left viewport.
  CHECK_FALSE(idle.subtitle_place.scrollable);
  CHECK(idle.subtitle_place.anchor == PASSPORT_ANCHOR_TOP_LEFT);
}
