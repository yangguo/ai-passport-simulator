#include <doctest/doctest.h>
#include "passport_sim/audio_pipeline_mock.h"
using passport_sim::AudioPipelineMock;
using passport_sim::AudioQueue;

TEST_CASE("depth accumulates and drain zeroes") {
  AudioPipelineMock a;
  a.packet(41, AudioQueue::Decode);
  a.packet(42, AudioQueue::Decode);
  CHECK(a.counters(AudioQueue::Decode).depth == 2);
  CHECK(a.counters(AudioQueue::Decode).gaps == 0);
  a.drain(AudioQueue::Decode);
  CHECK(a.counters(AudioQueue::Decode).depth == 0);
}

TEST_CASE("full queue drops and counts, stream position still advances") {
  AudioPipelineMock a;
  for (uint32_t s = 0; s < 40; ++s) a.packet(s, AudioQueue::Send);
  CHECK(a.counters(AudioQueue::Send).depth == 40);
  for (uint32_t s = 40; s < 45; ++s) a.packet(s, AudioQueue::Send);
  CHECK(a.counters(AudioQueue::Send).depth == 40);
  CHECK(a.counters(AudioQueue::Send).dropped == 5);
  CHECK(a.total_dropped() == 5);
  CHECK(a.counters(AudioQueue::Send).last_seq == 44);
}

TEST_CASE("gap, reorder, and retransmit counters are independent") {
  AudioPipelineMock a;
  a.packet(41, AudioQueue::Decode);
  a.packet(44, AudioQueue::Decode);  // jump 42..43
  CHECK(a.counters(AudioQueue::Decode).gaps == 1);
  a.mark_reordered(43);
  CHECK(a.counters(AudioQueue::Decode).reordered == 1);
  CHECK(a.counters(AudioQueue::Decode).gaps == 1);
  a.packet(43, AudioQueue::Decode);  // late/retransmit: no new gap
  CHECK(a.counters(AudioQueue::Decode).gaps == 1);
  CHECK(a.counters(AudioQueue::Decode).depth == 3);
}

TEST_CASE("status line format is exact") {
  AudioPipelineMock a;
  for (uint32_t s = 0; s < 40; ++s) a.packet(s, AudioQueue::Send);
  for (uint32_t s = 0; s < 12; ++s) a.packet(s, AudioQueue::Decode);
  for (uint32_t s = 40; s < 43; ++s) a.packet(s, AudioQueue::Send);
  CHECK(a.status_line() == "s 40/40 d 12/20 drop 3");
}
