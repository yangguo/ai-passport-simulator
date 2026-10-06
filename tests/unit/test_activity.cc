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

TEST_CASE("speaking clears to none on later idle, notifying counts as speaking") {
  CHECK(PassportNextActivity(PassportActivity::kSpeaking, kDeviceStateIdle) ==
        PassportActivity::kNone);
  CHECK(PassportNextActivity(PassportActivity::kNone, kDeviceStateNotifying) ==
        PassportActivity::kSpeaking);
  CHECK(PassportNextActivity(PassportActivity::kThinking, kDeviceStateIdle) ==
        PassportActivity::kNone);
  CHECK(PassportNextActivity(PassportActivity::kNone, kDeviceStateListening) ==
        PassportActivity::kListening);
  // Status bar already shows these; a second chip would duplicate them.
  CHECK(PassportActivityDuplicatesStatus(PassportActivity::kListening));
  CHECK_FALSE(PassportActivityDuplicatesStatus(PassportActivity::kNone));
}
