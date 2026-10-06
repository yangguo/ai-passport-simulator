#include <doctest/doctest.h>
#include "passport_sim/transport_mock.h"
using passport_sim::LinkState;
using passport_sim::TransportMock;

TEST_CASE("link down/up/recover cycle") {
  TransportMock t;
  CHECK(t.channel_open());
  CHECK_FALSE(t.has_error());
  t.link_down("wifi lost");
  CHECK_FALSE(t.channel_open());
  CHECK(t.has_error());
  CHECK(t.error_message() == "wifi lost");
  t.link_down("second reason");
  CHECK(t.error_message() == "wifi lost");  // first reason kept
  t.recover();
  CHECK(t.channel_open());
  CHECK(t.error_message().empty());
  t.recover();  // no-op while Up
  CHECK(t.channel_open());
}

TEST_CASE("timeout deadline fires once, disarmed by recover") {
  TransportMock t;
  t.timeout(1000, 500);
  CHECK(t.deadline_due(1200) == false);
  CHECK(t.deadline_due(1500) == true);
  CHECK_FALSE(t.poll_deadline(1200));
  CHECK(t.channel_open());
  CHECK(t.poll_deadline(1500));
  CHECK_FALSE(t.channel_open());
  CHECK_FALSE(t.poll_deadline(1600));  // fires once
  t.reset();
  CHECK(t.channel_open());
  t.timeout(0, 100);
  t.recover();
  CHECK_FALSE(t.poll_deadline(500));  // disarmed
}
