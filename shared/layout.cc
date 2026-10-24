#include "layout.h"

#include <cassert>

namespace passport_sim {

Layout::Layout(int width, int height, int radius, int line_height,
               int top_reserve, int emoji_half)
    : width_(width),
      height_(height),
      radius_(radius),
      line_height_(line_height),
      top_reserve_(top_reserve),
      emoji_half_idle_(emoji_half),
      emoji_half_caption_(0) {}

LayoutRects Layout::compute(PassportActivity activity) const {
  LayoutRects out;
  out.caption_priority = activity == PassportActivity::kThinking ||
                         activity == PassportActivity::kSpeaking;
  const int emoji_half =
      out.caption_priority ? emoji_half_caption_ : emoji_half_idle_;
  // Keep geometry calls outside assert(): NDEBUG strips assert expressions,
  // which would leave subtitle_viewport empty in Release builds.
  const bool safe_ok =
      passport_glass_safe_rect(width_, height_, radius_, &out.safe);
  const bool viewport_ok = passport_subtitle_viewport(
      width_, height_, radius_, line_height_, emoji_half, &out.subtitle_viewport);
  const bool activity_ok = passport_activity_line(
      &out.safe, &out.subtitle_viewport, line_height_, top_reserve_,
      &out.activity_line);
  assert(safe_ok);
  assert(viewport_ok);
  assert(activity_ok);
  (void)safe_ok;
  (void)viewport_ok;
  (void)activity_ok;
  passport_subtitle_bar_place(&out.subtitle_viewport, &out.subtitle_place);
  return out;
}

}  // namespace passport_sim
