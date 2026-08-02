#include "tamaink_renderer.h"
#include <cassert>
#include <cstdint>
#include <vector>

using tamaink::render::Rotation;
using tamaink::render::Status;
using tamaink::render::IconLayout;

static bool black(const std::vector<std::uint8_t>& b, std::size_t stride, unsigned x, unsigned y) {
  return (b[y * stride + x / 8] & (0x80u >> (x & 7u))) == 0;
}

int main() {
  tamaink::tamalib::Snapshot s{};
  s.lcd[0] = 1u;
  s.lcd[15] = 1u << 31;
  std::vector<std::uint8_t> b(256 + 4, 0xA5);
  assert(tamaink::render::snapshot(s, b.data(), 256, 64, 32, 8, 0, 0, 2) == Status::Ok);
  assert(b[0] == 0x3F);
  assert(b[31 * 8 + 7] == 0xFC);
  assert(b[256] == 0xA5 && b[259] == 0xA5);

  // CCW in y-down coordinates: (col,row) -> (row,31-col).
  tamaink::tamalib::Snapshot corners{};
  corners.lcd[0] = (1u << 0) | (1u << 31);
  corners.lcd[15] = (1u << 0) | (1u << 31);
  std::vector<std::uint8_t> rotated(64, 0xA5);
  assert(tamaink::render::snapshot(corners, rotated.data(), rotated.size(), 16, 32, 2, 0, 0, 1,
                                   Rotation::CounterClockwise90) == Status::Ok);
  assert(black(rotated, 2, 0, 0));   // top-right logical corner
  assert(black(rotated, 2, 0, 31));  // top-left logical corner
  assert(black(rotated, 2, 15, 0));  // bottom-right logical corner
  assert(black(rotated, 2, 15, 31)); // bottom-left logical corner
  assert(!black(rotated, 2, 8, 16));

  // Scaled cells retain a filled block and the rotated footprint is 16x32.
  tamaink::tamalib::Snapshot marker{};
  marker.lcd[3] = 1u << 7;
  std::vector<std::uint8_t> scaled(256, 0xA5);
  assert(tamaink::render::snapshot(marker, scaled.data(), scaled.size(), 32, 64, 4, 2, 3, 2,
                                   Rotation::CounterClockwise90) == Status::Ok);
  for (unsigned y = 3 + (31 - 7) * 2; y < 3 + (31 - 7) * 2 + 2; ++y)
    for (unsigned x = 2 + 3 * 2; x < 2 + 3 * 2 + 2; ++x) assert(black(scaled, 4, x, y));

  std::vector<std::uint8_t> clipped(64, 0xA5);
  tamaink::tamalib::Snapshot one{}; one.lcd[0] = 1u;
  assert(tamaink::render::snapshot(one, clipped.data(), clipped.size(), 16, 2, 2, -1, -1, 2) == Status::Ok);
  assert(black(clipped, 2, 0, 0));
  assert(tamaink::render::snapshot(s, clipped.data(), clipped.size(), 16, 2, 1, 0, 0, 1) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(s, clipped.data(), 1, 16, 2, 2, 0, 0, 1) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(s, clipped.data(), clipped.size(), 16, 2, 2, 0, 0, 0) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(s, nullptr, clipped.size(), 16, 2, 2, 0, 0, 1) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(s, clipped.data(), clipped.size(), 16, 32, 2, 0x7fffffff, 0, 1,
                                   Rotation::CounterClockwise90) == Status::Overflow);
  assert(tamaink::render::snapshot(s, clipped.data(), clipped.size(), 16, 32, 2, 0, 0, 1,
                                   static_cast<Rotation>(99)) == Status::InvalidArgument);

  // P1 icon layout: X3 geometry places centered 16x16 markers at x=236/540,
  // y=184+32*row. Inactive markers are outlines; active markers show glyphs.
  constexpr unsigned x3Width = 792, x3Height = 528, x3Stride = 99;
  std::vector<std::uint8_t> icons(x3Stride * x3Height, 0xA5);
  tamaink::tamalib::Snapshot empty{};
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   268, 8, 16, Rotation::CounterClockwise90,
                                   IconLayout::P1Margins) == Status::Ok);
  for (unsigned side = 0; side < 2; ++side) for (unsigned bit = 0; bit < 4; ++bit) {
    const unsigned x0 = side == 0 ? 236 : 540, y0 = 184 + bit * 32;
    assert(black(icons, x3Stride, x0, y0));
    assert(black(icons, x3Stride, x0 + 15, y0 + 15));
    assert(!black(icons, x3Stride, x0 + 7, y0 + 7));
  }
  // Each individual bit fills exactly its own marker interior and leaves LCD unchanged.
  for (unsigned bit = 0; bit < 8; ++bit) {
    tamaink::tamalib::Snapshot oneIcon{}; oneIcon.icons = static_cast<std::uint8_t>(1u << bit);
    std::vector<std::uint8_t> marked(x3Stride * x3Height, 0xA5);
    assert(tamaink::render::snapshot(oneIcon, marked.data(), marked.size(), x3Width, x3Height, x3Stride,
                                     268, 8, 16, Rotation::CounterClockwise90,
                                     IconLayout::P1Margins) == Status::Ok);
    const unsigned side = bit < 4 ? 0 : 1, row = 3u - (bit % 4u);
    const unsigned x0 = side == 0 ? 236 : 540, y0 = 184 + row * 32;
    // Active state contains its centered 5x5 project-owned glyph (2x2 pixels/cell)
    // with a clear 3px margin from the marker outline.
    static constexpr std::uint8_t glyphs[8][5] = {
      {0x04, 0x0E, 0x15, 0x04, 0x04}, {0x04, 0x0E, 0x1F, 0x0E, 0x04},
      {0x10, 0x18, 0x1C, 0x18, 0x10}, {0x04, 0x0E, 0x15, 0x04, 0x0E},
      {0x11, 0x0A, 0x04, 0x0A, 0x11}, {0x0E, 0x11, 0x15, 0x11, 0x0E},
      {0x1F, 0x11, 0x0A, 0x04, 0x04}, {0x04, 0x0E, 0x04, 0x00, 0x04}
    };
    for (unsigned gy = 0; gy < 5; ++gy) for (unsigned gx = 0; gx < 5; ++gx)
      for (unsigned py = 0; py < 2; ++py) for (unsigned px = 0; px < 2; ++px)
        assert(black(marked, x3Stride, x0 + 3 + gx * 2 + px, y0 + 3 + gy * 2 + py) ==
               ((glyphs[bit][gy] & (1u << (4u - gx))) != 0));
    assert(!black(marked, x3Stride, x0 + 1, y0 + 1));
    assert(!black(marked, x3Stride, x0 + 7, y0 + 16));
    // LCD footprint remains white when icons are the only source bits.
    for (unsigned y = 8; y < 520; ++y)
      for (unsigned x = 268; x < 524; ++x) assert(!black(marked, x3Stride, x, y));
  }
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   268, 8, 16, Rotation::None, IconLayout::P1Margins) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   -1000, -1000, 16, Rotation::CounterClockwise90,
                                   IconLayout::P1Margins) == Status::Ok);
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   268, 8, 16, Rotation::CounterClockwise90,
                                   static_cast<IconLayout>(99)) == Status::InvalidArgument);
  return 0;
}
