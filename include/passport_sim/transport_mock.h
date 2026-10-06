// Protocol-agnostic channel state machine (models firmware Protocol
// OnNetworkError/OnDisconnected/IsTimeout vocabulary). No sockets, no
// reconnect backoff, no clocks owned: the runner supplies virtual time.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace passport_sim {

enum class LinkState { Up, Down };

class TransportMock {
 public:
  TransportMock() = default;
  void link_down(std::string_view reason);
  void link_up();
  // Arms a deadline `after_ms` past `now_ms`; poll_deadline() fires it.
  void timeout(uint32_t now_ms, uint32_t after_ms);
  void error(std::string_view msg);  // Down + error string
  void recover();                    // Up, clears error
  bool channel_open() const;         // false iff Down
  bool has_error() const;
  std::string_view error_message() const;
  LinkState state() const;
  // Non-consuming query (for status display).
  bool deadline_due(uint32_t now_ms) const;
  // Returns true once when a pending deadline expires (then clears it).
  bool poll_deadline(uint32_t now_ms);
  void reset();

 private:
  LinkState state_ = LinkState::Up;
  std::string error_;
  bool deadline_armed_ = false;
  uint32_t deadline_at_ms_ = 0;
};

}  // namespace passport_sim
