#pragma once

#include <cstdint>

namespace tamaink::render {

struct Layout {
  std::uint16_t lcdScale;
  std::int32_t lcdOriginX;
  std::int32_t lcdOriginY;
  std::uint16_t iconScale;
};

// Compute the centered CCW LCD footprint from active native geometry. Icons
// intentionally retain their established 16x scale independently.
constexpr Layout layoutForGeometry(std::uint16_t width, std::uint16_t height) {
  std::uint16_t scale = 16;
  if (height < 512) {
    const auto fit = static_cast<std::uint16_t>((height > 32 ? height - 32 : 1) / 32);
    scale = fit < scale ? fit : scale;
    if (scale == 0) scale = 1;
  }
  const auto footprintWidth = static_cast<std::int32_t>(16u * scale);
  const auto footprintHeight = static_cast<std::int32_t>(32u * scale);
  return {scale, (static_cast<std::int32_t>(width) - footprintWidth) / 2,
          (static_cast<std::int32_t>(height) - footprintHeight) / 2, 16};
}

} // namespace tamaink::render
