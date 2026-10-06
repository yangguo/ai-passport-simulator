// Deterministic versioned-fixture replay on a VirtualClock.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "passport_activity.h"
#include "passport_sim/audio_pipeline_mock.h"
#include "passport_sim/button_input.h"
#include "passport_sim/conversation_events.h"
#include "passport_sim/settings_store.h"
#include "passport_sim/transport_mock.h"
#include "passport_sim/virtual_clock.h"

namespace passport_sim {

struct LoadResult {
  bool ok = false;
  std::string error;
};

// One validated fixture event. Only the fields required by `type` are set:
// state->value, stt/tts_sentence/note->text, key->key+action,
// transport->transport_action+reason/after_ms,
// audio_packet->audio_queue+seq+reorder.
struct ScenarioEvent {
  uint32_t at_ms = 0;
  std::string type;
  std::string text;
  std::string value;
  std::string key;
  std::string action;
  std::string transport_action;
  std::string reason;
  uint32_t after_ms = 0;
  std::string audio_queue;
  uint32_t seq = 0;
  bool reorder = false;
};

class ScenarioRunner {
 public:
  ScenarioRunner(VirtualClock& clock, CaptionBuffers& captions,
                 ButtonInput& buttons, SettingsStore& settings,
                 TransportMock& transport, AudioPipelineMock& audio);
  LoadResult load(const std::string& path);
  void reset();
  // Advances the clock to the next event, applies it, appends one log line.
  // Returns false when no events remain.
  bool step();
  void run();
  // Virtual time of the next pending event, or UINT32_MAX when done.
  // Used by interactive transports (play/pause) that advance the clock.
  uint32_t next_at_ms() const;
  std::string log() const { return log_; }
  std::string name() const { return name_; }
  PassportActivity activity() const { return activity_; }
  const CaptionBuffers& captions() const { return captions_; }
  const TransportMock& transport() const { return transport_; }
  const AudioPipelineMock& audio() const { return audio_; }

 private:
  VirtualClock& clock_;
  ConversationEvents conversation_;
  ButtonInput& buttons_;
  SettingsStore& settings_;
  CaptionBuffers& captions_;
  TransportMock& transport_;
  AudioPipelineMock& audio_;
  std::string name_;
  std::vector<ScenarioEvent> events_;
  size_t cursor_ = 0;
  PassportActivity activity_ = PassportActivity::kNone;
  bool channel_open_ = false;
  bool has_error_ = false;
  DeviceState last_device_state_ = kDeviceStateIdle;
  std::string log_;
};

}  // namespace passport_sim
