// Owned simulator module: separate bounded user/assistant caption buffers
// with the firmware STT-retention rules (see docs/REQUIREMENTS.md F-02).
#pragma once

#include <string>
#include <string_view>

namespace passport_sim {

struct CaptionLimits {
  size_t user_max_bytes = 512;
  size_t assistant_max_bytes = 2048;
};

class CaptionBuffers {
 public:
  explicit CaptionBuffers(CaptionLimits limits = {});
  // Returns false (keeps old text) for empty/invalid-UTF8 input.
  bool set_user_stt(std::string_view utf8);
  void begin_tts_turn();  // clears assistant only
  void append_tts_sentence(std::string_view utf8);  // space-separated append
  void on_state_or_system_event();  // must not touch either buffer
  std::string_view user() const { return user_; }
  std::string_view assistant() const { return assistant_; }
  bool user_truncated() const { return user_truncated_; }
  bool assistant_truncated() const { return assistant_truncated_; }
  void reset();

 private:
  CaptionLimits limits_;
  std::string user_;
  std::string assistant_;
  bool user_truncated_ = false;
  bool assistant_truncated_ = false;
};

}  // namespace passport_sim
