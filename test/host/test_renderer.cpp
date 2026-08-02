#include "tamaink_renderer.h"
#include <cassert>
#include <cstdint>
#include <algorithm>
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

  // P1 bottom-row layout: all 48x48 extents are at x=744; bit0 top=464,
  // bit7 top=16 (64px pitch), preserving perceived left-to-right order.
  constexpr unsigned x3Width = 792, x3Height = 528, x3Stride = 99;
  std::vector<std::uint8_t> icons(x3Stride * x3Height, 0xA5);
  tamaink::tamalib::Snapshot empty{};
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   268, 8, 16, Rotation::CounterClockwise90,
                                   IconLayout::P1BottomRow) == Status::Ok);
  for (unsigned bit = 0; bit < 8; ++bit) {
    const unsigned x0 = 744, y0 = 16 + (7 - bit) * 64;
    assert(!black(icons, x3Stride, x0, y0));
    assert(!black(icons, x3Stride, x0 + 47, y0 + 47));
    assert(!black(icons, x3Stride, x0 - 1, y0 + 24));
    assert(!black(icons, x3Stride, x0 + 47, y0 + 24));
    bool glyphVisible = false;
    for (unsigned y = y0 + 9; y < y0 + 39; ++y)
      for (unsigned x = x0 + 9; x < x0 + 39; ++x)
        glyphVisible = glyphVisible || black(icons, x3Stride, x, y);
    assert(glyphVisible);
  }
  // Each individual bit fills exactly its own marker interior and leaves LCD unchanged.
  for (unsigned bit = 0; bit < 8; ++bit) {
    tamaink::tamalib::Snapshot oneIcon{}; oneIcon.icons = static_cast<std::uint8_t>(1u << bit);
    std::vector<std::uint8_t> marked(x3Stride * x3Height, 0xA5);
    assert(tamaink::render::snapshot(oneIcon, marked.data(), marked.size(), x3Width, x3Height, x3Stride,
                                     268, 8, 16, Rotation::CounterClockwise90,
                                     IconLayout::P1BottomRow) == Status::Ok);
    const unsigned x0 = 744, y0 = 16 + (7 - bit) * 64;
    // Active state retains its centered glyph and adds open corner brackets.
    static constexpr std::uint8_t glyphs[8][5] = {
      {0x15, 0x1D, 0x09, 0x09, 0x09}, {0x15, 0x0E, 0x04, 0x0E, 0x0E},
      {0x18, 0x19, 0x02, 0x04, 0x08}, {0x03, 0x06, 0x0C, 0x18, 0x10},
      {0x11, 0x0E, 0x05, 0x0C, 0x1C}, {0x0E, 0x09, 0x09, 0x09, 0x0E},
      {0x0C, 0x0A, 0x0D, 0x0A, 0x0C}, {0x15, 0x1F, 0x0E, 0x04, 0x04}
    };
    for (unsigned gy = 0; gy < 5; ++gy) for (unsigned gx = 0; gx < 5; ++gx)
      for (unsigned py = 0; py < 6; ++py) for (unsigned px = 0; px < 6; ++px)
        assert(black(marked, x3Stride, x0 + 9 + gy * 6 + px, y0 + 9 + (4u - gx) * 6 + py) ==
               ((glyphs[bit][gy] & (1u << (4u - gx))) != 0));
    assert(black(marked, x3Stride, x0 + 3, y0 + 3));
    assert(black(marked, x3Stride, x0 + 5, y0 + 5)); // 3px-thick corner
    assert(black(marked, x3Stride, x0 + 14, y0 + 3)); // 12px arm
    assert(!black(marked, x3Stride, x0 + 15, y0 + 3));
    assert(!black(marked, x3Stride, x0 + 24, y0 + 3)); // open edge midpoint
    assert(black(marked, x3Stride, x0 + 44, y0 + 3));
    assert(black(marked, x3Stride, x0 + 42, y0 + 5));
    assert(black(marked, x3Stride, x0 + 33, y0 + 3));
    assert(!black(marked, x3Stride, x0 + 32, y0 + 3));
    assert(black(marked, x3Stride, x0 + 3, y0 + 44));
    assert(black(marked, x3Stride, x0 + 5, y0 + 42));
    assert(black(marked, x3Stride, x0 + 14, y0 + 44));
    assert(!black(marked, x3Stride, x0 + 15, y0 + 44));
    assert(black(marked, x3Stride, x0 + 44, y0 + 44));
    assert(black(marked, x3Stride, x0 + 42, y0 + 42));
    assert(black(marked, x3Stride, x0 + 33, y0 + 44));
    assert(!black(marked, x3Stride, x0 + 32, y0 + 44));
    assert(!black(marked, x3Stride, x0 + 24, y0 + 48)); // no spill
    assert(!black(marked, x3Stride, x0 - 1, y0 + 24));
    assert(!black(marked, x3Stride, x0 + 47, y0 + 24));
    // LCD footprint remains white when icons are the only source bits.
    for (unsigned y = 8; y < 520; ++y)
      for (unsigned x = 268; x < 524; ++x) assert(!black(marked, x3Stride, x, y));
  }
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   268, 8, 16, Rotation::None, IconLayout::P1BottomRow) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   -1000, -1000, 16, Rotation::CounterClockwise90,
                                   IconLayout::P1BottomRow) == Status::Ok);
  assert(tamaink::render::snapshot(empty, icons.data(), icons.size(), x3Width, x3Height, x3Stride,
                                   268, 8, 16, Rotation::CounterClockwise90,
                                   static_cast<IconLayout>(99)) == Status::InvalidArgument);

  // Portrait status bar geometry, thresholds, unknown state, and preservation.
  std::vector<std::uint8_t> statusBar(x3Stride * x3Height, 0xFF);
  const auto physicalBlack = [&](unsigned px, unsigned py) {
    return black(statusBar, x3Stride, py, x3Height - 1u - px);
  };
  assert(tamaink::render::snapshot(empty, statusBar.data(), statusBar.size(), x3Width, x3Height, x3Stride,
                                   268, 8, 16, Rotation::CounterClockwise90, IconLayout::P1BottomRow,
                                   tamaink::render::BatteryStatus{true, 97}) == Status::Ok);
  assert(physicalBlack(8, 17) && physicalBlack(79, 46));
  assert(physicalBlack(11, 20) && physicalBlack(76, 43)); // 4px border
  assert(!physicalBlack(12, 21)); // empty cavity beyond the border
  assert(physicalBlack(80, 28) && physicalBlack(87, 35));
  assert(!physicalBlack(250, 31) && !physicalBlack(300, 31)); // no divider
  for (unsigned bar = 0; bar < 5; ++bar) assert(physicalBlack(16 + bar * 12 + 2, 32));
  assert(physicalBlack(96, 17) && physicalBlack(132, 17)); // 30px-tall 97 digits
  // 9 and 7 use 3x5 glyphs at scale 6; no third glyph follows 97.
  assert(!physicalBlack(96, 35) && physicalBlack(108, 35));
  assert(!physicalBlack(120, 23) && physicalBlack(132, 23));
  assert(!physicalBlack(144, 17));
  assert(!physicalBlack(168, 20)); // no trailing percent glyph
  const unsigned boundaries[] = {0,1,20,21,40,41,60,61,80,81,100};
  for (unsigned v : boundaries) {
    std::fill(statusBar.begin(), statusBar.end(), 0xFF);
    assert(tamaink::render::snapshot(empty, statusBar.data(), statusBar.size(), x3Width, x3Height, x3Stride,
      268, 8, 16, Rotation::CounterClockwise90, IconLayout::P1BottomRow,
      tamaink::render::BatteryStatus{true, static_cast<std::uint8_t>(v)}) == Status::Ok);
    const unsigned expected = v == 0 ? 0 : (v - 1) / 20 + 1;
    for (unsigned bar = 0; bar < 5; ++bar)
      assert(physicalBlack(16 + bar * 12 + 2, 32) == (bar < expected));
  }
  std::fill(statusBar.begin(), statusBar.end(), 0xFF);
  assert(tamaink::render::snapshot(empty, statusBar.data(), statusBar.size(), x3Width, x3Height, x3Stride,
    268, 8, 16, Rotation::CounterClockwise90, IconLayout::P1BottomRow,
    tamaink::render::BatteryStatus{true, 100}) == Status::Ok);
  assert(physicalBlack(102, 17) && physicalBlack(120, 17) && physicalBlack(144, 17));
  std::fill(statusBar.begin(), statusBar.end(), 0xFF);
  assert(tamaink::render::snapshot(empty, statusBar.data(), statusBar.size(), x3Width, x3Height, x3Stride,
    268, 8, 16, Rotation::CounterClockwise90, IconLayout::P1BottomRow,
    tamaink::render::BatteryStatus{}) == Status::Ok);
  for (unsigned bar = 0; bar < 5; ++bar) assert(!physicalBlack(16 + bar * 12 + 2, 32));
  assert(physicalBlack(96, 29) && physicalBlack(120, 29)); // --
  assert(!black(statusBar, x3Stride, 100, 500)); // bounds
  assert(!black(statusBar, x3Stride, 300, 100));
  assert(!black(statusBar, x3Stride, 744, 16));
  return 0;
}
