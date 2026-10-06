// Owned simulator module: idle vs caption-priority viewport computation.
#pragma once

#include "passport_activity.h"
#include "screen_rounding.h"

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

}  // namespace passport_sim
