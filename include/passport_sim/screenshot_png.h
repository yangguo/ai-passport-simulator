// PNG writer for the 240x320 RGB565 viewport (stb_image_write).
#pragma once

#include <cstdint>
#include <string>

namespace passport_sim {

// Converts RGB565 rows to RGB888 and writes a PNG. Returns true on success.
bool SaveViewportPng(const std::string& path, const uint16_t* rgb565, int width,
                     int height);

}  // namespace passport_sim
