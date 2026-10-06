// LVGL presentation shell: owns the 240x320 display, screen objects, and the
// RGB565 framebuffer. Shared Layout/Caption modules drive every coordinate.
#pragma once

#include <cstdint>

#include "caption_buffers.h"
#include "passport_activity.h"

namespace passport_sim {

class LvglShell {
 public:
  static constexpr int kWidth = 240;
  static constexpr int kHeight = 320;
  // Subtitle page dwell, in virtual milliseconds (firmware kPassportSubtitlePageMs).
  static constexpr uint32_t kPageMs = 2500;

  LvglShell();
  ~LvglShell();
  LvglShell(const LvglShell&) = delete;
  LvglShell& operator=(const LvglShell&) = delete;

  // Recomputes layout from activity, updates all widgets, pumps LVGL once.
  void render(const CaptionBuffers& captions, PassportActivity activity);
  // Advances virtual time (page timer) and pumps LVGL timers.
  void tick(uint32_t ms);
  const uint16_t* framebuffer() const { return frame_; }

  // Alert overlay in the activity-line rect (hidden by default).
  void set_alert(const char* text);
  void clear_alert();

 private:
  void apply_page();
  static void MaskFrame();

  struct Impl;
  Impl* impl_;
  static uint16_t frame_[kWidth * kHeight];
};

}  // namespace passport_sim
