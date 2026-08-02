#include "tamaink_sleep_screen.h"

#include <cstring>
#include <limits>

namespace tamaink::sleep_screen {
namespace {

constexpr int kLogicalWidth = 528;
constexpr int kLogicalHeight = 792;

void drawPixel(std::uint8_t* buffer, std::size_t stride, int logicalX, int logicalY) {
  if (logicalX < 0 || logicalX >= kLogicalWidth || logicalY < 0 || logicalY >= kLogicalHeight) return;

  // The X3 panel is native landscape. This is the same confirmed CCW mapping
  // used by the live P1 renderer: logical portrait (x,y) -> native (y,527-x).
  const int nativeX = logicalY;
  const int nativeY = kLogicalWidth - 1 - logicalX;
  auto& byte = buffer[static_cast<std::size_t>(nativeY) * stride + static_cast<std::size_t>(nativeX) / 8u];
  const auto mask = static_cast<std::uint8_t>(0x80u >> (nativeX & 7));
  byte &= static_cast<std::uint8_t>(~mask);
}

struct Glyph {
  char character;
  std::uint8_t rows[7];
};

constexpr Glyph kFont[] = {
    {'T', {31, 4, 4, 4, 4, 4, 4}},       {'a', {0, 0, 14, 1, 15, 17, 15}},
    {'m', {0, 0, 26, 21, 21, 17, 17}},   {'I', {31, 4, 4, 4, 4, 4, 31}},
    {'n', {0, 0, 30, 17, 17, 17, 17}},   {'k', {17, 18, 20, 24, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'N', {17, 25, 21, 19, 17, 17, 17}}, {'G', {14, 17, 16, 23, 17, 17, 14}},
};

const Glyph* findGlyph(char character) {
  for (const auto& glyph : kFont) {
    if (glyph.character == character) return &glyph;
  }
  return nullptr;
}

void drawText(std::uint8_t* buffer, std::size_t stride, int logicalY, const char* text, int scale, int gap,
              bool bold) {
  int width = 0;
  for (const char* cursor = text; *cursor; ++cursor) width += 5 * scale + gap;
  width -= gap;
  int logicalX = (kLogicalWidth - width) / 2;

  for (const char* cursor = text; *cursor; ++cursor) {
    const Glyph* glyph = findGlyph(*cursor);
    if (glyph) {
      for (int glyphY = 0; glyphY < 7; ++glyphY) {
        for (int glyphX = 0; glyphX < 5; ++glyphX) {
          if ((glyph->rows[glyphY] & (1u << (4 - glyphX))) == 0) continue;
          for (int pixelY = 0; pixelY < scale; ++pixelY) {
            for (int pixelX = 0; pixelX < scale; ++pixelX) {
              const int x = logicalX + glyphX * scale + pixelX;
              const int y = logicalY + glyphY * scale + pixelY;
              drawPixel(buffer, stride, x, y);
              if (bold) drawPixel(buffer, stride, x + 1, y);
            }
          }
        }
      }
    }
    logicalX += 5 * scale + gap;
  }
}

void drawLogo(std::uint8_t* buffer, std::size_t stride) {
  constexpr int centerX = kLogicalWidth / 2;
  constexpr int centerY = 180;

  // Original egg/ink-drop outline. The widening upper half reads as an egg;
  // the tighter lower tip and small ink oval make it specific to TamaInk.
  for (int y = -42; y <= 42; ++y) {
    const int halfWidth = y < 0 ? 18 + (y + 42) / 3 : 18 + (42 - y) / 3;
    for (int x = -halfWidth; x <= halfWidth; ++x) {
      if (x == -halfWidth || x == halfWidth || y == -42 || y == 42) {
        drawPixel(buffer, stride, centerX + x, centerY + y);
      }
    }
  }
  for (int y = -10; y <= 10; ++y) {
    for (int x = -14; x <= 14; ++x) {
      if (x * x * 100 + y * y * 196 < 19600) drawPixel(buffer, stride, centerX + x, centerY + y);
    }
  }
  for (int x = -8; x <= 8; ++x) drawPixel(buffer, stride, centerX + x, centerY - 2);
}

}  // namespace

Status render(std::uint8_t* destination, std::size_t capacity, std::uint16_t width, std::uint16_t height,
              std::size_t stride) {
  if (!destination || width != 792 || height != 528 || stride < (width + 7u) / 8u) {
    return Status::InvalidArgument;
  }
  if (height > std::numeric_limits<std::size_t>::max() / stride) return Status::Overflow;
  const std::size_t bytes = stride * height;
  if (bytes > capacity) return Status::InvalidArgument;

  std::memset(destination, 0xFF, bytes);
  drawLogo(destination, stride);
  drawText(destination, stride, 270, "TamaInk", 6, 6, true);
  drawText(destination, stride, 360, "SLEEPING", 3, 5, false);
  return Status::Ok;
}

}  // namespace tamaink::sleep_screen
