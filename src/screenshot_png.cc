#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "passport_sim/screenshot_png.h"

#include <vector>

namespace passport_sim {

bool SaveViewportPng(const std::string& path, const uint16_t* rgb565, int width,
                     int height) {
  std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const uint16_t p = rgb565[y * width + x];
      uint8_t* out = &rgb[(static_cast<size_t>(y) * width + x) * 3];
      out[0] = static_cast<uint8_t>(((p >> 11) & 0x1F) * 255 / 31);
      out[1] = static_cast<uint8_t>(((p >> 5) & 0x3F) * 255 / 63);
      out[2] = static_cast<uint8_t>((p & 0x1F) * 255 / 31);
    }
  }
  return stbi_write_png(path.c_str(), width, height, 3, rgb.data(),
                        width * 3) != 0;
}

}  // namespace passport_sim
