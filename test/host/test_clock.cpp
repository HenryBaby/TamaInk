#include "tamaink_clock.h"
#include <cassert>
#include <cstdint>
int main() {
  tamaink::clock::ClockState c; c.reset(1000, 123);
  assert(c.now(1000) == 123); assert(c.now(1000 + 1000000) == 32891);
  c.enterFast(); c.enterFast(); assert(c.now(0xFFFFFFFFu) == 123);
  assert(c.fastAdvanceTo(0xFFFFFFFFu)); assert(c.fastAdvanceTo(2));
  assert(c.timestamp() == 2); assert(c.exitFast(77)); assert(!c.fast());
  assert(c.now(77 + 1000000) == 32770); assert(!c.fastAdvanceTo(4));
  c.reset(0xFFFFFF00u, 10); assert(c.now(0x100u) == 10 + (512u * 32768u / 1000000u));
  return 0;
}
