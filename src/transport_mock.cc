#include "passport_sim/transport_mock.h"

namespace passport_sim {

void TransportMock::link_down(std::string_view reason) {
  if (state_ == LinkState::Down) return;  // keep first reason
  state_ = LinkState::Down;
  error_ = std::string(reason);
}

void TransportMock::link_up() {
  state_ = LinkState::Up;
  error_.clear();
  deadline_armed_ = false;
}

void TransportMock::timeout(uint32_t now_ms, uint32_t after_ms) {
  deadline_armed_ = true;
  deadline_at_ms_ = now_ms + after_ms;
}

void TransportMock::error(std::string_view msg) {
  state_ = LinkState::Down;
  error_ = std::string(msg);
}

void TransportMock::recover() { link_up(); }

bool TransportMock::channel_open() const {
  return state_ != LinkState::Down;
}

bool TransportMock::has_error() const { return !error_.empty(); }

std::string_view TransportMock::error_message() const { return error_; }

LinkState TransportMock::state() const { return state_; }

bool TransportMock::deadline_due(uint32_t now_ms) const {
  return deadline_armed_ && now_ms >= deadline_at_ms_;
}

bool TransportMock::poll_deadline(uint32_t now_ms) {
  if (!deadline_due(now_ms)) return false;
  deadline_armed_ = false;
  if (state_ == LinkState::Up) {
    state_ = LinkState::Down;
    error_ = "timeout";
    return true;
  }
  return false;
}

void TransportMock::reset() {
  state_ = LinkState::Up;
  error_.clear();
  deadline_armed_ = false;
  deadline_at_ms_ = 0;
}

}  // namespace passport_sim
