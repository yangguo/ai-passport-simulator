// Task 2: glass geometry unit tests.
//
// Mirrors the intent of FoloToy/ai-passport tests/test_bsp_display_rounding.c
// (row-span vs mask equivalence, corners, clamped/degenerate radii, edge
// rejections). The firmware module dropped the per-pixel helper, so edge rows
// are asserted against the official pixel semantics instead:
// at 240x320 r30, row 0 is visible exactly from x=30.
#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "screen_rounding.h"

TEST_CASE("rounded row span matches official corner semantics at 240x320 r30") {
  const int32_t W = 240, H = 320, R = 30;
  int32_t x1 = -1, x2 = -1;
  REQUIRE(passport_rounded_row_span(0, W, H, R, &x1, &x2));
  CHECK(x1 == 30);
  CHECK(x2 == W - 1 - 30);
  // Middle rows are full width.
  REQUIRE(passport_rounded_row_span(H / 2, W, H, R, &x1, &x2));
  CHECK(x1 == 0);
  CHECK(x2 == W - 1);
  // Symmetry top/bottom.
  int32_t bx1 = -1, bx2 = -1;
  REQUIRE(passport_rounded_row_span(H - 1, W, H, R, &bx1, &bx2));
  CHECK(bx1 == 30);
  CHECK(bx2 == W - 1 - 30);
}

TEST_CASE("row span rejects bad input") {
  int32_t x1 = 123, x2 = 456;
  CHECK_FALSE(passport_rounded_row_span(-1, 240, 320, 30, &x1, &x2));
  CHECK_FALSE(passport_rounded_row_span(320, 240, 320, 30, &x1, &x2));
  CHECK_FALSE(passport_rounded_row_span(0, 0, 320, 30, &x1, &x2));
  CHECK_FALSE(passport_rounded_row_span(0, 240, 320, 30, nullptr, &x2));
  CHECK_FALSE(passport_rounded_row_span(0, 240, 320, 30, &x1, nullptr));
  // Degenerate radii clamp instead of failing.
  REQUIRE(passport_rounded_row_span(5, 17, 11, 0, &x1, &x2));
  CHECK(x1 == 0);
  CHECK(x2 == 16);
}

TEST_CASE("glass safe rect is full-width and inside the glass") {
  passport_rect_t safe{};
  REQUIRE(passport_glass_safe_rect(240, 320, 30, &safe));
  CHECK(safe.width == 240);
  CHECK(passport_rect_inside_glass(&safe, 240, 320, 30));
  // Every safe row spans the full width.
  for (int32_t y = safe.y; y < safe.y + safe.height; ++y) {
    int32_t x1 = -1, x2 = -1;
    REQUIRE(passport_rounded_row_span(y, 240, 320, 30, &x1, &x2));
    CHECK(x1 == 0);
    CHECK(x2 == 239);
  }
  // Corners are outside the glass.
  passport_rect_t corner{0, 0, 1, 1};
  passport_rect_t edge{30, 0, 1, 1};
  CHECK_FALSE(passport_rect_inside_glass(&corner, 240, 320, 30));
  CHECK(passport_rect_inside_glass(&edge, 240, 320, 30));
}

TEST_CASE("subtitle viewport fits in safe rect with whole lines") {
  passport_rect_t sub{};
  REQUIRE(passport_subtitle_viewport(240, 320, 30, 16, 16, &sub));
  passport_rect_t safe{};
  REQUIRE(passport_glass_safe_rect(240, 320, 30, &safe));
  CHECK(sub.x >= safe.x);
  CHECK(sub.y >= safe.y);
  CHECK(sub.x + sub.width <= safe.x + safe.width);
  CHECK(sub.y + sub.height <= safe.y + safe.height);
  CHECK(sub.height % 16 == 0);
  CHECK(passport_rect_inside_glass(&sub, 240, 320, 30));
  CHECK(passport_subtitle_page_count(100, sub.height) >= 1);
  CHECK(passport_subtitle_page_offset(0, 100, sub.height) == 0);
}

TEST_CASE("rgb565 mask zeroes corners and keeps center") {
  const int32_t W = 240, H = 320, R = 30;
  std::vector<uint8_t> buf(W * H * 2, 0xFF);
  passport_mask_rgb565_area(buf.data(), W * 2, 0, 0, W - 1, H - 1, W, H, R);
  auto pixel = [&](int x, int y) -> uint16_t {
    const auto* p = buf.data() + (y * W + x) * 2;
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
  };
  CHECK(pixel(0, 0) == 0);
  CHECK(pixel(29, 0) == 0);
  CHECK(pixel(30, 0) != 0);
  CHECK(pixel(120, 160) != 0);
  CHECK(pixel(239, 319) == 0);
}
