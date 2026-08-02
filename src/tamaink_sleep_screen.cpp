#include "tamaink_sleep_screen.h"

#include <FreeInkUIFont.h>

#include <cstring>
#include <limits>

namespace tamaink::sleep_screen {
namespace {

constexpr int kLogicalWidth = 528;
constexpr int kLogicalHeight = 792;

void drawPixel(std::uint8_t* buffer, std::size_t stride, int logicalX, int logicalY) {
  if (logicalX < 0 || logicalX >= kLogicalWidth || logicalY < 0 || logicalY >= kLogicalHeight) return;
  const int nativeX = logicalY;
  const int nativeY = kLogicalWidth - 1 - logicalX;
  auto& byte = buffer[static_cast<std::size_t>(nativeY) * stride + static_cast<std::size_t>(nativeX) / 8u];
  byte &= static_cast<std::uint8_t>(~(0x80u >> (nativeX & 7)));
}

void drawHorizontal(std::uint8_t* buffer, std::size_t stride, int x, int y, int width, int thickness) {
  for (int py = 0; py < thickness; ++py) {
    for (int px = 0; px < width; ++px) drawPixel(buffer, stride, x + px, y + py);
  }
}

void drawVertical(std::uint8_t* buffer, std::size_t stride, int x, int y, int height, int thickness) {
  for (int px = 0; px < thickness; ++px) {
    for (int py = 0; py < height; ++py) drawPixel(buffer, stride, x + px, y + py);
  }
}

bool isPetCutout(int x, int y) {
  if (y >= -48 && y <= -31) return x >= -55 && x <= 15;
  if (y >= -30 && y <= -13) return x >= -55 && x <= 32;
  if (y >= -12 && y <= 5) return x >= -55 && x <= 48;
  if (y >= 6 && y <= 48) return x >= -55 && x <= 55;
  if (y >= 49 && y <= 61) return x >= -40 && x <= 55;
  if (y >= 62 && y <= 72) return x >= -20 && x <= 48;
  return false;
}

void drawClosedEye(std::uint8_t* buffer, std::size_t stride, int centerX, int y) {
  drawVertical(buffer, stride, centerX - 12, y, 7, 4);
  drawHorizontal(buffer, stride, centerX - 8, y + 6, 16, 4);
  drawVertical(buffer, stride, centerX + 8, y, 7, 4);
}

void drawSleepZ(std::uint8_t* buffer, std::size_t stride, int x, int y) {
  drawHorizontal(buffer, stride, x, y, 18, 4);
  for (int step = 0; step < 5; ++step) {
    drawHorizontal(buffer, stride, x + 12 - step * 3, y + 4 + step * 3, 6, 4);
  }
  drawHorizontal(buffer, stride, x, y + 19, 18, 4);
}

void drawLogo(std::uint8_t* buffer, std::size_t stride) {
  constexpr int centerX = kLogicalWidth / 2;
  constexpr int centerY = 343;

  // A filled egg with a stepped upper-right opening, matching the reference's
  // solid silhouette while remaining deterministic on a 1-bit panel.
  for (int y = -105; y <= 105; ++y) {
    for (int x = -112; x <= 112; ++x) {
      if (x * x * 105 + y * y * 112 > 112 * 112 * 105) continue;
      if (x > 45 && y < -35 + (x - 45) / 2) continue;
      if (isPetCutout(x, y)) continue;
      drawPixel(buffer, stride, centerX + x, centerY + y);
    }
  }

  drawClosedEye(buffer, stride, centerX - 28, centerY + 12);
  drawClosedEye(buffer, stride, centerX + 28, centerY + 12);
  drawSleepZ(buffer, stride, centerX + 22, centerY - 42);
}

const freeink::ui::FontGlyph* fontGlyph(char character) {
  const auto codepoint = static_cast<unsigned char>(character);
  const auto& font = freeink::ui::kNotoSansFont;
  if (codepoint < font.first || codepoint > font.last) return nullptr;
  return &font.glyphs[codepoint - font.first];
}

int textWidth(const char* text, int scale, int tracking) {
  int width = 0;
  bool first = true;
  for (const char* cursor = text; *cursor; ++cursor) {
    const auto* glyph = fontGlyph(*cursor);
    if (!glyph) continue;
    if (!first) width += tracking;
    width += glyph->xAdvance * scale;
    first = false;
  }
  return width;
}

void drawText(std::uint8_t* buffer, std::size_t stride, int baseline, const char* text, int scale, int tracking,
              bool heavy) {
  const auto& font = freeink::ui::kNotoSansFont;
  int penX = (kLogicalWidth - textWidth(text, scale, tracking)) / 2;
  bool first = true;

  for (const char* cursor = text; *cursor; ++cursor) {
    const auto* glyph = fontGlyph(*cursor);
    if (!glyph) continue;
    if (!first) penX += tracking;
    first = false;

    const std::uint8_t* bitmap = font.bitmap + glyph->bitmapOffset;
    for (int glyphY = 0; glyphY < glyph->height; ++glyphY) {
      for (int glyphX = 0; glyphX < glyph->width; ++glyphX) {
        const int bit = glyphY * glyph->width + glyphX;
        if ((bitmap[bit / 8] & (0x80u >> (bit & 7))) == 0) continue;
        for (int pixelY = 0; pixelY < scale; ++pixelY) {
          for (int pixelX = 0; pixelX < scale; ++pixelX) {
            const int x = penX + glyph->xOffset * scale + glyphX * scale + pixelX;
            const int y = baseline + glyph->yOffset * scale + glyphY * scale + pixelY;
            drawPixel(buffer, stride, x, y);
            if (heavy) {
              drawPixel(buffer, stride, x + 1, y);
              drawPixel(buffer, stride, x, y + 1);
            }
          }
        }
      }
    }
    penX += glyph->xAdvance * scale;
  }
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
  drawText(destination, stride, 513, "TamaInk", 2, 4, true);
  drawText(destination, stride, 555, "SLEEPING", 1, 8, false);
  return Status::Ok;
}

}  // namespace tamaink::sleep_screen
