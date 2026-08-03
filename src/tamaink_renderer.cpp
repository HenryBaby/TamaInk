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
    if (battery.visible) {
      auto pixelPhysical = [&](std::int64_t x, std::int64_t y, bool black) {
        pixel(y, static_cast<std::int64_t>(height) - 1 - x, black);
      };
      constexpr std::uint8_t digits[11][5] = {
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},{0,0,7,0,0}
      };
      constexpr std::int64_t digitScaleX = 6;
      constexpr std::int64_t digitHeight = 28;
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
        for (std::int64_t x = iconX + iconW; x < iconX + iconW + 8; ++x)
          pixelPhysical(x, y, true);
      const unsigned bars = !battery.percentageKnown || battery.percentage > 100 ? 0u :
        (battery.percentage == 0 ? 0u : (battery.percentage - 1u) / 20u + 1u);
      for (unsigned bar = 0; bar < 5; ++bar) {
        const std::int64_t bx = iconX + 8 + static_cast<std::int64_t>(bar) * 12;
        for (std::int64_t y = iconY + 8; y < iconY + iconH - 8; ++y)
          for (std::int64_t x = bx; x < bx + 8; ++x) pixelPhysical(x, y, bar < bars);
      }
      const unsigned value = battery.percentageKnown && battery.percentage <= 100 ? battery.percentage : 0;
      const unsigned tens = value / 10, ones = value % 10;
      const std::int64_t textY = 18, textStart = 96, digitPitch = 24;
      auto glyph = [&](unsigned digit, std::int64_t x0) {
        for (unsigned gy = 0; gy < 5; ++gy) for (unsigned gx = 0; gx < 3; ++gx)
          if (digits[digit][gy] & (1u << (2u - gx)))
            for (std::int64_t py = gy * digitHeight / 5; py < (gy + 1) * digitHeight / 5; ++py)
              for (std::int64_t px = 0; px < digitScaleX; ++px)
                pixelPhysical(x0 + gx * digitScaleX + px, textY + py, true);
      };
      std::int64_t textX = textStart;
      if (battery.percentageKnown && battery.percentage <= 100) {
        if (value < 10) glyph(ones, textX);
        else if (value < 100) { glyph(tens, textX); glyph(ones, textX + digitPitch); }
        else { glyph(1, textX); glyph(0, textX + digitPitch); glyph(0, textX + 2 * digitPitch); }
      } else { glyph(10, textX); glyph(10, textX + digitPitch); }
    }
  }
  return Status::Ok;
}

Status overlaySettings(std::uint8_t* destination, std::size_t capacity,
                       std::uint16_t width, std::uint16_t height,
                       std::size_t stride, const settings::Values& values,
                       std::uint8_t focus, bool confirmation, bool resetYes) {
  if (!destination || !width || !height || focus > 4 ||
      stride < (static_cast<std::size_t>(width) + 7u) / 8u ||
      height > std::numeric_limits<std::size_t>::max() / stride ||
      stride * static_cast<std::size_t>(height) > capacity || !settings::validate(values))
    return Status::InvalidArgument;

  // The menu is designed in logical CCW portrait coordinates, then mapped to
  // the native framebuffer (x=native y, y=height-1-logical x).
  const int logicalWidth = height;
  const int logicalHeight = width;
  const int boxWidth = 432;
  const int boxHeight = 340;
  const int boxX = (logicalWidth - boxWidth) / 2;
  const int boxY = (logicalHeight - boxHeight) / 2;
  auto pixel = [&](int logicalX, int logicalY, bool black) {
    if (logicalX < 0 || logicalY < 0 || logicalX >= logicalWidth || logicalY >= logicalHeight)
      return;
    const int nativeX = logicalY;
    const int nativeY = static_cast<int>(height) - 1 - logicalX;
    auto& byte = destination[static_cast<std::size_t>(nativeY) * stride +
                             static_cast<std::size_t>(nativeX) / 8u];
    const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (nativeX & 7));
    if (black) byte &= static_cast<std::uint8_t>(~mask);
    else byte |= mask;
  };
  for (int y = boxY; y < boxY + boxHeight; ++y)
    for (int x = boxX; x < boxX + boxWidth; ++x) pixel(x, y, false);
  for (int t = 0; t < 6; ++t) {
    for (int x = boxX; x < boxX + boxWidth; ++x) {
      pixel(x, boxY + t, true); pixel(x, boxY + boxHeight - 1 - t, true);
    }
    for (int y = boxY; y < boxY + boxHeight; ++y) {
      pixel(boxX + t, y, true); pixel(boxX + boxWidth - 1 - t, y, true);
    }
  }

  static constexpr std::uint8_t font[26][5] = {
    {0x1E,0x05,0x1F,0x11,0x1F},{0x1E,0x11,0x1E,0x11,0x1E},{0x0F,0x10,0x10,0x10,0x0F},
    {0x1E,0x11,0x11,0x11,0x1E},{0x1F,0x10,0x1E,0x10,0x1F},{0x1F,0x10,0x1E,0x10,0x10},
    {0x0F,0x10,0x17,0x11,0x0F},{0x11,0x11,0x1F,0x11,0x11},{0x1F,0x04,0x04,0x04,0x1F},
    {0x01,0x01,0x01,0x11,0x0E},{0x11,0x12,0x1C,0x12,0x11},{0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x11,0x11},{0x11,0x19,0x15,0x13,0x11},{0x0E,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x1E,0x10,0x10},{0x0E,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x1E,0x12,0x11},
    {0x0F,0x10,0x0E,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04},{0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x0A,0x04},{0x11,0x11,0x15,0x1B,0x11},{0x11,0x0A,0x04,0x0A,0x11},
    {0x11,0x0A,0x04,0x04,0x04},{0x1F,0x02,0x04,0x08,0x1F}
  };
  auto drawText = [&](const char* text, int x, int y, bool black) {
    constexpr int scale = 4;
    for (const char* p = text; *p; ++p, x += 24) {
      if (*p == ' ') continue;
      const std::uint8_t* glyph = nullptr;
      static constexpr std::uint8_t digits[10][5] = {
        {0x0E,0x11,0x13,0x15,0x0E},{0x04,0x0C,0x04,0x04,0x0E},{0x0E,0x11,0x02,0x04,0x1F},
        {0x1E,0x01,0x06,0x01,0x1E},{0x02,0x06,0x0A,0x1F,0x02},{0x1F,0x10,0x1E,0x01,0x1E},
        {0x06,0x08,0x1E,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x04},{0x0E,0x11,0x0E,0x11,0x0E},
        {0x0E,0x11,0x0F,0x01,0x06}
      };
      if (*p >= 'A' && *p <= 'Z') glyph = font[*p - 'A'];
      else if (*p >= '0' && *p <= '9') glyph = digits[*p - '0'];
      if (!glyph) continue;
      for (int gy = 0; gy < 5; ++gy)
        for (int gx = 0; gx < 5; ++gx)
          if (glyph[gy] & (1u << (4 - gx)))
            for (int py = 0; py < scale; ++py)
              for (int px = 0; px < scale; ++px) pixel(x + gx * scale + px, y + gy * scale + py, black);
    }
  };
  if (confirmation) {
    drawText("RESET", boxX + 156, boxY + 32, true);
    drawText("DELETE DATA?", boxX + 72, boxY + 100, true);
    drawText("CANNOT UNDO", boxX + 72, boxY + 140, true);
    const int optionY = boxY + 212;
    const int noX = boxX + 92;
    const int yesX = boxX + 244;
    const int selectedX = resetYes ? yesX - 16 : noX - 16;
    const int selectedWidth = resetYes ? 104 : 80;
    for (int y = optionY - 10; y < optionY + 30; ++y)
      for (int x = selectedX; x < selectedX + selectedWidth; ++x) pixel(x, y, true);
    drawText("NO", noX, optionY, resetYes);
    drawText("YES", yesX, optionY, !resetYes);
    return Status::Ok;
  }
  // SETTINGS header is centered using measured 24-pixel glyph pitch.
  drawText("SETTINGS", boxX + (boxWidth - 8 * 24 + 4) / 2, boxY + 24, true);
  const char* labels[] = {"BATTERY", "DISPLAY", "AUTOSAVE", "RESET", "EXIT"};
  const char* battery[] = {"SHOW", "HIDE"};
  const char* display[] = {"SMOOTH", "BALANCED", "ECO"};
  const char* autosave[] = {"5 MIN", "15 MIN", "30 MIN"};
  const char* valuesText = nullptr;
  const int rowHeight = 48;
  for (int row = 0; row < 5; ++row) {
    const int rowY = boxY + 76 + row * rowHeight;
    const bool selected = row == focus;
    if (selected)
      for (int y = rowY - 8; y < rowY + 28; ++y)
        for (int x = boxX + 12; x < boxX + boxWidth - 12; ++x) pixel(x, y, true);
    drawText(labels[row], boxX + 32, rowY, !selected);
    if (row == 0) valuesText = battery[values.battery == settings::Battery::Hide];
    else if (row == 1) valuesText = display[static_cast<int>(values.display)];
    else if (row == 2) valuesText = autosave[static_cast<int>(values.autosave)];
    else valuesText = "";
    drawText(valuesText, boxX + 228, rowY, !selected);
  }
  return Status::Ok;
}
} // namespace tamaink::render
