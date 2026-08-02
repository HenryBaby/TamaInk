#include "tamaink_autosave.h"
#include <cassert>
int main() {
  using namespace tamaink::autosave;
  Scheduler s{}; s.armed = true; s.next = 1000;
  assert(!due(s, 999)); assert(due(s, 1000));
  s.immediate = false; request(s); assert(due(s, 0));
  success(s, 2000, 900); assert(!s.immediate && s.next == 2900 && !due(s, 2001));
  failure(s, 3000, 60); assert(s.next == 3060 && !s.immediate);
  s.next = 0xfffffff0u; s.armed = true; assert(!due(s, 0xffffffefu)); assert(due(s, 0xfffffff0u)); assert(due(s, 0x10u));
  Controller c{}; c.scheduler.armed = true; c.scheduler.next = 100;
  assert(c.tick(99, true, false) == Action::None); assert(c.tick(100, true, false) == Action::Begin);
  assert(!c.manualAllowed(false)); assert(c.advance(false) == Action::Advance);
  c.successCommit(100, 1000); assert(c.manualAllowed(false)); assert(c.scheduler.next == 1100);
  assert(c.manualAllowed(false)); assert(!c.manualAllowed(true));
  c.tick(1100, true, false); c.writeFailure(1100, 60); assert(c.scheduler.next == 1160 && !c.automatic);
  request(c.scheduler); assert(c.tick(0, true, true) == Action::None); assert(c.tick(0, true, false) == Action::Begin);
  c.cleanup(0, 900); assert(!c.scheduler.immediate && c.scheduler.next == 900);
  c.successCommit(0xfffffff0u, 32); assert(c.scheduler.next == 0x10u); assert(c.tick(0x0fu, true, false) == Action::None); assert(c.tick(0x10u, true, false) == Action::Begin);
  c.writeFailure(0x20u, 60); assert(c.scheduler.next == 0x5cu);
  return 0;
}

