// SIMULATED conversation-event injection (STT/TTS/state). All values are
// fixture-driven; nothing here measures real audio or network.
#pragma once

#include <string_view>

#include "caption_buffers.h"

namespace passport_sim {

class ConversationEvents {
 public:
  explicit ConversationEvents(CaptionBuffers& captions) : captions_(captions) {}
  // Returns false (keeps old text) for empty/invalid input.
  bool stt(std::string_view utf8) { return captions_.set_user_stt(utf8); }
  void tts_start() { captions_.begin_tts_turn(); }
  void tts_sentence(std::string_view utf8) {
    captions_.append_tts_sentence(utf8);
  }
  void tts_stop() {}
  // State changes and system messages must not touch caption buffers.
  void system_event() { captions_.on_state_or_system_event(); }

 private:
  CaptionBuffers& captions_;
};

}  // namespace passport_sim
