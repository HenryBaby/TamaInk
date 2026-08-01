#include "tamaink_renderer.h"
#include <cassert>
#include <cstdint>
#include <vector>

using tamaink::render::Rotation;
using tamaink::render::Status;

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
  return 0;
}
