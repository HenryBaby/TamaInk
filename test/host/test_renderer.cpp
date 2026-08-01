#include "tamaink_renderer.h"
#include <cassert>
#include <cstdint>
#include <vector>
using tamaink::render::Status;
int main() {
  tamaink::tamalib::Snapshot s{}; s.lcd[0] = 1u; s.lcd[15] = 1u << 31;
  std::vector<std::uint8_t> b(256 + 4, 0xA5);
  assert(tamaink::render::snapshot(s, b.data(), 256, 64, 32, 8, 0, 0, 2) == Status::Ok);
  assert(b[0] == 0x3F); assert(b[31 * 8 + 7] == 0xFC);
  assert(b[256] == 0xA5 && b[259] == 0xA5);
  std::vector<std::uint8_t> c(64, 0xA5);
  tamaink::tamalib::Snapshot clipped{}; clipped.lcd[0] = 1u;
  assert(tamaink::render::snapshot(clipped, c.data(), c.size(), 16, 2, 2, -1, -1, 2) == Status::Ok);
  assert(c[0] == 0x7F);
  assert(tamaink::render::snapshot(s, c.data(), c.size(), 16, 2, 1, 0, 0, 1) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(s, c.data(), 1, 16, 2, 2, 0, 0, 1) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(s, c.data(), c.size(), 16, 2, 2, 0, 0, 0) == Status::InvalidArgument);
  assert(tamaink::render::snapshot(s, nullptr, c.size(), 16, 2, 2, 0, 0, 1) == Status::InvalidArgument);
  return 0;
}
