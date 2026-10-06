// Monotonic millisecond clock driven by fixtures, never wall-clock.
#pragma once

#include <cstdint>

namespace passport_sim {

class VirtualClock {
 public:
  VirtualClock() = default;
  // Advances to ms; going backwards is a bug (asserts in debug).
  void advance_to(uint32_t ms);
  uint32_t now_ms() const { return now_ms_; }
  void reset() { now_ms_ = 0; }

 private:
  uint32_t now_ms_ = 0;
};

}  // namespace passport_sim
