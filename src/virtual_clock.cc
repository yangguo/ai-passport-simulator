#include "passport_sim/virtual_clock.h"

#include <cassert>

namespace passport_sim {

void VirtualClock::advance_to(uint32_t ms) {
  // Fixture times are monotonic; going backwards is a caller bug.
  assert(ms >= now_ms_);
  now_ms_ = ms;
}

}  // namespace passport_sim
