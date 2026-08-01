#include "tamaink_renderer.h"
#include <cstring>
#include <limits>

namespace tamaink::render {
Status snapshot(const tamalib::Snapshot& source, std::uint8_t* destination,
               std::size_t capacity, std::uint16_t width, std::uint16_t height,
               std::size_t stride, std::int32_t originX, std::int32_t originY,
               std::uint16_t scale) {
  if (!destination || !width || !height || !scale || stride < (static_cast<std::size_t>(width) + 7u) / 8u)
    return Status::InvalidArgument;
  if (height > std::numeric_limits<std::size_t>::max() / stride) return Status::Overflow;
  const std::size_t bytes = stride * height;
  if (bytes > capacity) return Status::InvalidArgument;
  std::memset(destination, 0xFF, bytes);
  const std::int64_t right = static_cast<std::int64_t>(originX) + 32ll * scale;
  const std::int64_t bottom = static_cast<std::int64_t>(originY) + 16ll * scale;
  if (right < std::numeric_limits<std::int32_t>::min() || bottom < std::numeric_limits<std::int32_t>::min())
    return Status::Overflow;
  for (std::uint16_t row = 0; row < 16; ++row) {
    const std::uint32_t bits = source.lcd[row];
    for (std::uint16_t col = 0; col < 32; ++col) {
      if ((bits & (1u << col)) == 0) continue;
      const std::int64_t x0 = static_cast<std::int64_t>(originX) + static_cast<std::int64_t>(col) * scale;
      const std::int64_t y0 = static_cast<std::int64_t>(originY) + static_cast<std::int64_t>(row) * scale;
      const std::int64_t x1 = x0 + scale, y1 = y0 + scale;
      const std::int64_t cx0 = x0 < 0 ? 0 : x0, cy0 = y0 < 0 ? 0 : y0;
      const std::int64_t cx1 = x1 > width ? width : x1, cy1 = y1 > height ? height : y1;
      for (std::int64_t y = cy0; y < cy1; ++y)
        for (std::int64_t x = cx0; x < cx1; ++x)
          destination[static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) / 8u] &=
              static_cast<std::uint8_t>(~(0x80u >> (static_cast<unsigned>(x) & 7u)));
    }
  }
  return Status::Ok;
}
} // namespace tamaink::render
