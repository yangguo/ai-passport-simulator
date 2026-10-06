#include "passport_sim/button_input.h"

namespace passport_sim {

void ButtonInput::press(Button button) {
  if (down_) return;  // already held; keep the first press time
  if (button != pending_button_) pending_single_ = false;
  down_ = true;
  down_button_ = button;
  down_at_ms_ = clock_.now_ms();
  last_event_ = ButtonEvent::Press;
  last_button_ = button;
}

void ButtonInput::release(Button button) {
  if (!down_ || button != down_button_) return;
  down_ = false;
  const uint32_t held = clock_.now_ms() - down_at_ms_;
  last_button_ = button;
  if (held >= kLongPressMs) {
    last_event_ = ButtonEvent::Long;
    pending_single_ = false;
  } else if (pending_single_ && pending_button_ == button &&
             clock_.now_ms() - last_release_ms_ <= kDoubleClickMs) {
    last_event_ = ButtonEvent::Double;
    pending_single_ = false;
  } else {
    last_event_ = ButtonEvent::Click;
    pending_single_ = true;
    pending_button_ = button;
  }
  last_release_ms_ = clock_.now_ms();
}

void ButtonInput::reset() {
  down_ = false;
  last_event_ = ButtonEvent::None;
  last_button_ = Button::Ok;
  pending_single_ = false;
  pending_button_ = Button::Ok;
  last_release_ms_ = 0;
}

}  // namespace passport_sim
