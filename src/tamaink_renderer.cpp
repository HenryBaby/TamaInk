#include "tamaink_renderer.h"
#include <cstring>
#include <limits>

namespace tamaink::render {
Status snapshot(const tamalib::Snapshot& source, std::uint8_t* destination,
               std::size_t capacity, std::uint16_t width, std::uint16_t height,
               std::size_t stride, std::int32_t originX, std::int32_t originY,
               std::uint16_t scale, Rotation rotation, IconLayout iconLayout,
               BatteryStatus battery) {
  if (!destination || !width || !height || !scale || stride < (static_cast<std::size_t>(width) + 7u) / 8u)
    return Status::InvalidArgument;
  if (rotation != Rotation::None && rotation != Rotation::CounterClockwise90)
    return Status::InvalidArgument;
  if (iconLayout != IconLayout::None && iconLayout != IconLayout::P1BottomRow)
    return Status::InvalidArgument;
  if (iconLayout == IconLayout::P1BottomRow && rotation != Rotation::CounterClockwise90)
    return Status::InvalidArgument;
  if (height > std::numeric_limits<std::size_t>::max() / stride) return Status::Overflow;
  const std::size_t bytes = stride * height;
  if (bytes > capacity) return Status::InvalidArgument;
  std::memset(destination, 0xFF, bytes);
  const std::int64_t footprintWidth = (rotation == Rotation::CounterClockwise90 ? 16ll : 32ll) * scale;
  const std::int64_t footprintHeight = (rotation == Rotation::CounterClockwise90 ? 32ll : 16ll) * scale;
  const std::int64_t right = static_cast<std::int64_t>(originX) + footprintWidth;
  const std::int64_t bottom = static_cast<std::int64_t>(originY) + footprintHeight;
  if (right < std::numeric_limits<std::int32_t>::min() || right > std::numeric_limits<std::int32_t>::max() ||
      bottom < std::numeric_limits<std::int32_t>::min() || bottom > std::numeric_limits<std::int32_t>::max())
    return Status::Overflow;
  for (std::uint16_t row = 0; row < 16; ++row) {
    const std::uint32_t bits = source.lcd[row];
    for (std::uint16_t col = 0; col < 32; ++col) {
      if ((bits & (1u << col)) == 0) continue;
      // In y-down coordinates, CCW maps (col,row) to (row, 31-col).
      const std::int64_t x0 = static_cast<std::int64_t>(originX) +
          static_cast<std::int64_t>(rotation == Rotation::CounterClockwise90 ? row : col) * scale;
      const std::int64_t y0 = static_cast<std::int64_t>(originY) +
          static_cast<std::int64_t>(rotation == Rotation::CounterClockwise90 ? 31 - col : row) * scale;
      const std::int64_t x1 = x0 + scale, y1 = y0 + scale;
      const std::int64_t cx0 = x0 < 0 ? 0 : x0, cy0 = y0 < 0 ? 0 : y0;
      const std::int64_t cx1 = x1 > width ? width : x1, cy1 = y1 > height ? height : y1;
      for (std::int64_t y = cy0; y < cy1; ++y)
        for (std::int64_t x = cx0; x < cx1; ++x)
          destination[static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) / 8u] &=
              static_cast<std::uint8_t>(~(0x80u >> (static_cast<unsigned>(x) & 7u)));
    }
  }
  if (iconLayout == IconLayout::P1BottomRow) {
    const std::int64_t markerSide = scale * 3ll;
    const std::int64_t markerStep = scale * 4ll;
    const std::int64_t markerY = static_cast<std::int64_t>(originY) + scale / 2ll;
    const std::int64_t markerX = static_cast<std::int64_t>(width) - markerSide;
    auto pixel = [&](std::int64_t x, std::int64_t y, bool black) {
      if (x < 0 || y < 0 || x >= width || y >= height) return;
      auto& byte = destination[static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) / 8u];
      const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (static_cast<unsigned>(x) & 7u));
      if (black) byte &= static_cast<std::uint8_t>(~mask); else byte |= mask;
    };
    // Small original monochrome glyphs, expressed as 5x5 bitmaps rather than
    // copied device artwork. Bit order follows TamaLib/P1 menu semantics:
    // Food, Light, Game, Medicine, Toilet, Health, Discipline, Attention.
    // Glyphs use the same CCW convention as the LCD: source (gx,gy) maps to
    // destination (gy,4-gx) in y-down coordinates.
    static constexpr std::uint8_t glyphs[8][5] = {
      {0x15, 0x1D, 0x09, 0x09, 0x09}, {0x15, 0x0E, 0x04, 0x0E, 0x0E},
      {0x18, 0x19, 0x02, 0x04, 0x08}, {0x03, 0x06, 0x0C, 0x18, 0x10},
      {0x11, 0x0E, 0x05, 0x0C, 0x1C}, {0x0E, 0x09, 0x09, 0x09, 0x0E},
      {0x0C, 0x0A, 0x0D, 0x0A, 0x0C}, {0x15, 0x1F, 0x0E, 0x04, 0x04}
    };
    for (unsigned row = 0; row < 8; ++row) {
      const std::int64_t y0 = markerY + static_cast<std::int64_t>(7u - row) * markerStep;
      const std::int64_t x0 = markerX;
      const unsigned iconBit = row;
      const bool active = (source.icons & (1u << iconBit)) != 0;
      // Keep the symbol visible in both states; active adds four short,
      // open corner brackets rather than a placeholder box.
      const std::int64_t cell = markerSide / 8ll > 0 ? markerSide / 8ll : 1ll;
      const std::int64_t glyphOriginX = x0 + (markerSide - cell * 5ll) / 2ll;
      const std::int64_t glyphOriginY = y0 + (markerSide - cell * 5ll) / 2ll;
      for (unsigned gy = 0; gy < 5; ++gy)
        for (unsigned gx = 0; gx < 5; ++gx)
          if ((glyphs[iconBit][gy] & (1u << (4u - gx))) != 0)
            for (std::int64_t py = 0; py < cell; ++py)
              for (std::int64_t px = 0; px < cell; ++px)
                pixel(glyphOriginX + gy * cell + px, glyphOriginY + (4u - gx) * cell + py, true);
      if (active) {
        constexpr std::int64_t inset = 3, thickness = 3, length = 12;
        for (std::int64_t t = 0; t < thickness; ++t)
          for (std::int64_t i = 0; i < length; ++i) {
            pixel(x0 + inset + i, y0 + inset + t, true);
            pixel(x0 + inset + t, y0 + inset + i, true);
            pixel(x0 + markerSide - inset - 1 - i, y0 + inset + t, true);
            pixel(x0 + markerSide - inset - 1 - t, y0 + inset + i, true);
            pixel(x0 + inset + i, y0 + markerSide - inset - 1 - t, true);
            pixel(x0 + inset + t, y0 + markerSide - inset - 1 - i, true);
            pixel(x0 + markerSide - inset - 1 - i, y0 + markerSide - inset - 1 - t, true);
            pixel(x0 + markerSide - inset - 1 - t, y0 + markerSide - inset - 1 - i, true);
          }
      }
    }

    // X3 portrait status bar: deliberately independent of the LCD footprint
    // so the pet and original bottom-row icons remain untouched.
    auto pixelPhysical = [&](std::int64_t x, std::int64_t y, bool black) {
      pixel(y, static_cast<std::int64_t>(height) - 1 - x, black);
    };
    constexpr std::uint8_t digits[11][5] = {
      {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
      {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},{0,0,7,0,0}
    };
    constexpr std::int64_t digitScale = 6;
    // Top status region is 64 physical pixels high; no divider is drawn.
    constexpr std::int64_t iconX = 8, iconY = 17, iconW = 72, iconH = 30;
    constexpr std::int64_t border = 4;
    for (std::int64_t t = 0; t < border; ++t) {
      for (std::int64_t x = iconX; x < iconX + iconW; ++x) {
        pixelPhysical(x, iconY + t, true);
        pixelPhysical(x, iconY + iconH - 1 - t, true);
      }
      for (std::int64_t y = iconY; y < iconY + iconH; ++y) {
        pixelPhysical(iconX + t, y, true);
        pixelPhysical(iconX + iconW - 1 - t, y, true);
      }
    }
    for (std::int64_t y = iconY + 11; y < iconY + 19; ++y)
      for (std::int64_t x = iconX + iconW; x < iconX + iconW + 8; ++x) pixelPhysical(x, y, true);
    const unsigned bars = !battery.percentageKnown || battery.percentage > 100 ? 0u :
      (battery.percentage == 0 ? 0u : (battery.percentage - 1u) / 20u + 1u);
    for (unsigned bar = 0; bar < 5; ++bar) {
      const std::int64_t bx = iconX + 8 + static_cast<std::int64_t>(bar) * 12;
      for (std::int64_t y = iconY + 8; y < iconY + iconH - 8; ++y)
        for (std::int64_t x = bx; x < bx + 8; ++x) pixelPhysical(x, y, bar < bars);
    }
    const unsigned value = battery.percentageKnown && battery.percentage <= 100 ? battery.percentage : 0;
    const unsigned tens = value / 10, ones = value % 10;
    const std::int64_t textY = 17, textStart = 96, digitPitch = 24;
    auto glyph = [&](unsigned digit, std::int64_t x0) {
      for (unsigned gy = 0; gy < 5; ++gy) for (unsigned gx = 0; gx < 3; ++gx)
        if (digits[digit][gy] & (1u << (2u - gx)))
          for (std::int64_t py = 0; py < digitScale; ++py)
            for (std::int64_t px = 0; px < digitScale; ++px)
              pixelPhysical(x0 + gx * digitScale + px, textY + gy * digitScale + py, true);
    };
    std::int64_t textX = textStart;
    if (battery.percentageKnown && battery.percentage <= 100) {
      if (value < 10) glyph(ones, textX);
      else if (value < 100) { glyph(tens, textX); glyph(ones, textX + digitPitch); }
      else { glyph(1, textX); glyph(0, textX + digitPitch); glyph(0, textX + 2 * digitPitch); }
    } else { glyph(10, textX); glyph(10, textX + digitPitch); }
  }
  return Status::Ok;
}
} // namespace tamaink::render
