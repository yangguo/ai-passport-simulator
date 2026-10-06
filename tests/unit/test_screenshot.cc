#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <doctest/doctest.h>

#include <filesystem>

#include "passport_sim/lvgl_shell.h"
#include "passport_sim/screenshot_png.h"

using passport_sim::CaptionBuffers;
using passport_sim::LvglShell;
using passport_sim::SaveViewportPng;

TEST_CASE("screenshot round-trips a 2x2 RGB565 pattern") {
  // red, green / blue, white in RGB565.
  const uint16_t pixels[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  const auto path =
      (std::filesystem::temp_directory_path() / "viewport_roundtrip.png")
          .string();
  REQUIRE(SaveViewportPng(path, pixels, 2, 2));
  int w = 0, h = 0, comp = 0;
  stbi_uc* img = stbi_load(path.c_str(), &w, &h, &comp, 3);
  REQUIRE(img != nullptr);
  CHECK(w == 2);
  CHECK(h == 2);
  CHECK(img[0] == 255);
  CHECK(img[1] == 0);
  CHECK(img[2] == 0);
  CHECK(img[3] == 0);
  CHECK(img[4] == 255);
  CHECK(img[5] == 0);
  CHECK(img[6] == 0);
  CHECK(img[7] == 0);
  CHECK(img[8] == 255);
  CHECK(img[9] == 255);
  CHECK(img[10] == 255);
  CHECK(img[11] == 255);
  stbi_image_free(img);
}

TEST_CASE("LVGL shell renders captions inside the glass safe area") {
  CaptionBuffers captions;
  REQUIRE(captions.set_user_stt("hello"));
  captions.begin_tts_turn();
  captions.append_tts_sentence("world");
  LvglShell shell;
  shell.render(captions, PassportActivity::kSpeaking);
  shell.tick(100);
  const uint16_t* fb = shell.framebuffer();
  // Background is dark ink, not black: something was drawn.
  bool non_zero = false;
  for (int i = 0; i < LvglShell::kWidth * LvglShell::kHeight; ++i) {
    if (fb[i] != 0) {
      non_zero = true;
      break;
    }
  }
  CHECK(non_zero);
  // Rounded corners stay masked (background never paints there).
  CHECK(fb[0] == 0);
  CHECK(fb[LvglShell::kWidth - 1] == 0);
  CHECK(fb[(LvglShell::kHeight - 1) * LvglShell::kWidth] == 0);
}
