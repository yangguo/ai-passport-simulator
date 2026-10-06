#include "caption_buffers.h"

namespace passport_sim {
namespace {

bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

}  // namespace

bool utf8_valid(std::string_view s) {
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t len = 1;
    if ((c & 0x80) == 0)
      len = 1;
    else if ((c & 0xE0) == 0xC0)
      len = 2;
    else if ((c & 0xF0) == 0xE0)
      len = 3;
    else if ((c & 0xF8) == 0xF0)
      len = 4;
    else
      return false;
    if (i + len > s.size()) return false;
    for (size_t k = 1; k < len; ++k)
      if (!is_continuation(static_cast<unsigned char>(s[i + k]))) return false;
    i += len;
  }
  return true;
}

namespace {

std::string truncate_at_utf8_boundary(std::string_view s, size_t max_bytes) {
  if (s.size() <= max_bytes) return std::string(s);
  size_t n = max_bytes;
  while (n > 0 && is_continuation(static_cast<unsigned char>(s[n]))) --n;
  return std::string(s.substr(0, n));
}

}  // namespace

CaptionBuffers::CaptionBuffers(CaptionLimits limits) : limits_(limits) {}

bool CaptionBuffers::set_user_stt(std::string_view utf8) {
  if (utf8.empty() || !utf8_valid(utf8)) return false;
  user_ = truncate_at_utf8_boundary(utf8, limits_.user_max_bytes);
  user_truncated_ = utf8.size() > limits_.user_max_bytes;
  return true;
}

void CaptionBuffers::begin_tts_turn() {
  assistant_.clear();
  assistant_truncated_ = false;
}

void CaptionBuffers::append_tts_sentence(std::string_view utf8) {
  if (utf8.empty() || !utf8_valid(utf8)) return;
  if (!assistant_.empty()) assistant_ += " ";
  assistant_ += utf8;
  if (assistant_.size() > limits_.assistant_max_bytes) {
    assistant_ =
        truncate_at_utf8_boundary(assistant_, limits_.assistant_max_bytes);
    assistant_truncated_ = true;
  }
}

void CaptionBuffers::on_state_or_system_event() {
  // Intentionally a no-op: state changes and empty system messages must not
  // erase either caption buffer (REQUIREMENTS F-02).
}

void CaptionBuffers::reset() {
  user_.clear();
  assistant_.clear();
  user_truncated_ = false;
  assistant_truncated_ = false;
}

}  // namespace passport_sim
