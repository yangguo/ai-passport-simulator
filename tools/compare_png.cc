// Pixel-diff for golden screenshots.
// Exit 0 identical (within threshold), 1 different, 2 usage/IO error.
// Threshold: at most 0.1% of pixels may differ, where a pixel differs when
// max(|dr|,|dg|,|db|) > 8. Writes a diff PNG (red = different).
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool LoadRgb(const char* path, int& w, int& h, std::vector<uint8_t>& px) {
  int comp = 0;
  stbi_uc* img = stbi_load(path, &w, &h, &comp, 3);
  if (!img) {
    std::fprintf(stderr, "compare-png: cannot load %s\n", path);
    return false;
  }
  px.assign(img, img + static_cast<size_t>(w) * h * 3);
  stbi_image_free(img);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    std::fprintf(stderr, "usage: compare-png <expected> <actual> <diff-out>\n");
    return 2;
  }
  int ew = 0, eh = 0, aw = 0, ah = 0;
  std::vector<uint8_t> epx, apx;
  if (!LoadRgb(argv[1], ew, eh, epx) || !LoadRgb(argv[2], aw, ah, apx)) {
    return 2;
  }
  if (ew != aw || eh != ah) {
    std::fprintf(stderr, "compare-png: size mismatch %dx%d vs %dx%d\n", ew,
                 eh, aw, ah);
    return 1;
  }
  const size_t total = static_cast<size_t>(ew) * eh;
  size_t differ = 0;
  std::vector<uint8_t> diff(total * 3, 0);
  for (size_t i = 0; i < total; ++i) {
    int d = 0;
    for (int c = 0; c < 3; ++c) {
      d = std::max(d, std::abs(static_cast<int>(epx[i * 3 + c]) -
                               static_cast<int>(apx[i * 3 + c])));
    }
    if (d > 8) {
      ++differ;
      diff[i * 3] = 255;
    }
  }
  const double ratio = total == 0 ? 0.0 : static_cast<double>(differ) / total;
  std::printf("compare-png: %zu/%zu pixels differ (%.4f%%)\n", differ, total,
              ratio * 100.0);
  if (ratio > 0.001) {
    stbi_write_png(argv[3], ew, eh, 3, diff.data(), ew * 3);
    return 1;
  }
  return 0;
}
