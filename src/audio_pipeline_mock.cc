#include "passport_sim/audio_pipeline_mock.h"

namespace passport_sim {
namespace {

uint32_t capacity_for(const QueueCapacities& caps, AudioQueue q) {
  switch (q) {
    case AudioQueue::Encode:
      return caps.encode;
    case AudioQueue::Send:
      return caps.send;
    case AudioQueue::Decode:
      return caps.decode;
    case AudioQueue::Playback:
      return caps.playback;
  }
  return 0;
}

int index_for(AudioQueue q) { return static_cast<int>(q); }

}  // namespace

AudioPipelineMock::AudioPipelineMock(QueueCapacities caps) : caps_(caps) {
  per_queue_[index_for(AudioQueue::Encode)].capacity = caps_.encode;
  per_queue_[index_for(AudioQueue::Send)].capacity = caps_.send;
  per_queue_[index_for(AudioQueue::Decode)].capacity = caps_.decode;
  per_queue_[index_for(AudioQueue::Playback)].capacity = caps_.playback;
}

void AudioPipelineMock::packet(uint32_t seq, AudioQueue q) {
  QueueCounters& c = per_queue_[index_for(q)];
  if (c.has_seq && seq > c.last_seq + 1) ++c.gaps;
  c.has_seq = true;
  c.last_seq = seq;
  if (c.depth >= c.capacity) {
    ++c.dropped;
    return;
  }
  ++c.depth;
}

void AudioPipelineMock::mark_reordered([[maybe_unused]] uint32_t seq) {
  ++per_queue_[index_for(AudioQueue::Decode)].reordered;
}

void AudioPipelineMock::drain(AudioQueue q) {
  per_queue_[index_for(q)].depth = 0;
}

const QueueCounters& AudioPipelineMock::counters(AudioQueue q) const {
  return per_queue_[index_for(q)];
}

uint32_t AudioPipelineMock::total_dropped() const {
  uint32_t total = 0;
  for (const auto& c : per_queue_) total += c.dropped;
  return total;
}

std::string AudioPipelineMock::status_line() const {
  const QueueCounters& s = per_queue_[index_for(AudioQueue::Send)];
  const QueueCounters& d = per_queue_[index_for(AudioQueue::Decode)];
  return "s " + std::to_string(s.depth) + "/" + std::to_string(s.capacity) +
         " d " + std::to_string(d.depth) + "/" + std::to_string(d.capacity) +
         " drop " + std::to_string(total_dropped());
}

void AudioPipelineMock::reset() {
  for (auto& c : per_queue_) {
    c.depth = 0;
    c.dropped = 0;
    c.reordered = 0;
    c.gaps = 0;
    c.last_seq = 0;
    c.has_seq = false;
  }
}

}  // namespace passport_sim
