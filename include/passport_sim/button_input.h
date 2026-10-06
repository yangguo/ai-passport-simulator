// UP/DOWN/OK virtual button synthesis (event taxonomy mirrors the official
// bsp_button.h PRESS/CLICK/DOUBLE/LONG; ADC voltages and debounce are
// explicitly NOT simulated).
#pragma once

#include <cstdint>

#include "passport_sim/virtual_clock.h"

namespace passport_sim {

enum class Button { Up, Down, Ok };
enum class ButtonEvent { None, Press, Click, Double, Long };

// Long-press threshold in virtual milliseconds.
inline constexpr uint32_t kLongPressMs = 600;
// Max gap between two clicks to count as DOUBLE, in virtual milliseconds.
inline constexpr uint32_t kDoubleClickMs = 400;

class ButtonInput {
 public:
  explicit ButtonInput(VirtualClock& clock) : clock_(clock) {}
  void press(Button button);
  // Synthesizes Click/Double/Long from press duration and history.
  void release(Button button);
  ButtonEvent last_event() const { return last_event_; }
  Button last_button() const { return last_button_; }
  void reset();

 private:
  VirtualClock& clock_;
  bool down_ = false;
  Button down_button_ = Button::Ok;
  uint32_t down_at_ms_ = 0;
  ButtonEvent last_event_ = ButtonEvent::None;
  Button last_button_ = Button::Ok;
  bool pending_single_ = false;
  Button pending_button_ = Button::Ok;
  uint32_t last_release_ms_ = 0;
};

}  // namespace passport_sim
