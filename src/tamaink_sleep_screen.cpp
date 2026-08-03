#include "tamaink_sleep_screen.h"

#include <FreeInkUIFont.h>

#include <cstring>
#include <limits>

namespace tamaink::sleep_screen {
namespace {

void drawPixel(std::uint8_t* buffer, std::size_t stride, int logicalWidth, int logicalHeight,
               int logicalX, int logicalY) {
  if (logicalX < 0 || logicalX >= logicalWidth || logicalY < 0 || logicalY >= logicalHeight) return;
  const int nativeX = logicalY;
  const int nativeY = logicalWidth - 1 - logicalX;
  auto& byte = buffer[static_cast<std::size_t>(nativeY) * stride + static_cast<std::size_t>(nativeX) / 8u];
  byte &= static_cast<std::uint8_t>(~(0x80u >> (nativeX & 7)));
}

void drawHorizontal(std::uint8_t* buffer, std::size_t stride, int logicalWidth, int logicalHeight, int x, int y, int width, int thickness) {
  for (int py = 0; py < thickness; ++py) {
    for (int px = 0; px < width; ++px) drawPixel(buffer, stride, logicalWidth, logicalHeight, x + px, y + py);
  }
}

void drawVertical(std::uint8_t* buffer, std::size_t stride, int logicalWidth, int logicalHeight, int x, int y, int height, int thickness) {
  for (int px = 0; px < thickness; ++px) {
    for (int py = 0; py < height; ++py) drawPixel(buffer, stride, logicalWidth, logicalHeight, x + px, y + py);
  }
}

void drawClosedEye(std::uint8_t* buffer, std::size_t stride, int logicalWidth, int logicalHeight, int centerX, int y) {
  drawVertical(buffer, stride, logicalWidth, logicalHeight, centerX - 12, y, 7, 4);
  drawHorizontal(buffer, stride, logicalWidth, logicalHeight, centerX - 8, y + 6, 16, 4);
  drawVertical(buffer, stride, logicalWidth, logicalHeight, centerX + 8, y, 7, 4);
}

void drawSleepZ(std::uint8_t* buffer, std::size_t stride, int logicalWidth, int logicalHeight, int x, int y) {
  drawHorizontal(buffer, stride, logicalWidth, logicalHeight, x, y, 18, 4);
  for (int step = 0; step < 5; ++step) {
    drawHorizontal(buffer, stride, logicalWidth, logicalHeight, x + 12 - step * 3, y + 4 + step * 3, 6, 4);
  }
  drawHorizontal(buffer, stride, logicalWidth, logicalHeight, x, y + 19, 18, 4);
}

void drawSleepingFace(std::uint8_t* buffer, std::size_t stride, int logicalWidth, int logicalHeight) {
  // Minimal reference composition: two closed eyes with a pair of rising,
  // stepped sleep symbols. There is intentionally no enclosing body or egg.
  const int cx = logicalWidth / 2;
  const int top = (logicalHeight - 792) / 2 + 238;
  drawSleepZ(buffer, stride, logicalWidth, logicalHeight, cx + 38, top);
  drawSleepZ(buffer, stride, logicalWidth, logicalHeight, cx + 22, top + 36);
  drawClosedEye(buffer, stride, logicalWidth, logicalHeight, cx - 28, top + 92);
  drawClosedEye(buffer, stride, logicalWidth, logicalHeight, cx + 28, top + 92);
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

void drawText(std::uint8_t* buffer, std::size_t stride, int logicalWidth, int logicalHeight, int baseline, const char* text, int scale, int tracking,
              bool heavy) {
  const auto& font = freeink::ui::kNotoSansFont;
  int penX = (logicalWidth - textWidth(text, scale, tracking)) / 2;
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
            drawPixel(buffer, stride, logicalWidth, logicalHeight, x, y);
            if (heavy) {
              drawPixel(buffer, stride, logicalWidth, logicalHeight, x + 1, y);
              drawPixel(buffer, stride, logicalWidth, logicalHeight, x, y + 1);
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
  const bool supportedGeometry = (width == 792 && height == 528) || (width == 800 && height == 480);
  if (!destination || !supportedGeometry ||
      stride < (width + 7u) / 8u) {
    return Status::InvalidArgument;
  }
  if (height > std::numeric_limits<std::size_t>::max() / stride) return Status::Overflow;
  const std::size_t bytes = stride * height;
  if (bytes > capacity) return Status::InvalidArgument;

  std::memset(destination, 0xFF, bytes);
  drawSleepingFace(destination, stride, height, width);
  drawText(destination, stride, height, width, static_cast<int>(width) - 279, "TamaInk", 2, 4, true);
  drawText(destination, stride, height, width, static_cast<int>(width) - 237, "SLEEPING", 1, 8, false);
  return Status::Ok;
}

}  // namespace tamaink::sleep_screen
