// Per-packet accounting for the 4-queue audio pipeline (mirrors firmware
// audio_service.h: encode/send/decode/playback, FixedQueue drop-on-full).
// Counts only: packet bytes are never stored (deliberate YAGNI).
#pragma once

#include <cstdint>
#include <string>

namespace passport_sim {

enum class AudioQueue { Encode, Send, Decode, Playback };

// Measured firmware capacities (audio_service.h): encode/playback tasks 2,
// send packets 2400/60ms, decode packets 1200/60ms.
struct QueueCapacities {
  uint32_t encode = 2;
  uint32_t send = 40;
  uint32_t decode = 20;
  uint32_t playback = 2;
};

struct QueueCounters {
  uint32_t depth = 0;
  uint32_t capacity = 0;
  uint32_t dropped = 0;
  uint32_t reordered = 0;
  uint32_t gaps = 0;
  uint32_t last_seq = 0;
  bool has_seq = false;
};

class AudioPipelineMock {
 public:
  explicit AudioPipelineMock(QueueCapacities caps = {});
  // Enqueue one packet; a full queue drops it and counts (never blocks).
  void packet(uint32_t seq, AudioQueue q);
  // Counts a reordered arrival (decode-side); seq kept for log detail.
  void mark_reordered([[maybe_unused]] uint32_t seq);
  void drain(AudioQueue q);  // zeroes depth only; totals persist
  const QueueCounters& counters(AudioQueue q) const;
  uint32_t total_dropped() const;
  // Exact: "s <send-depth>/<send-cap> d <dec-depth>/<dec-cap> drop <total>"
  std::string status_line() const;
  void reset();

 private:
  QueueCapacities caps_;
  QueueCounters per_queue_[4];
};

}  // namespace passport_sim
