#include <cassert>
#include "tamaink_wake_diagnostic.h"
#include "tamaink_sleep_gesture.h"
int main() {
  tamaink::wake::Coordinator c;
  assert(c.request(true,true,true,false,false));
  assert(!c.request(true,true,true,false,false));
  assert(!c.mutationAllowed(false));
  assert(c.mutationAllowed(true));
  assert(c.saveSucceeded());
  assert(!c.saveSucceeded());
  assert(c.request(true,true,true,false,false)); c.saveFailed();
  assert(c.request(true,true,true,false,false)); c.armFailed();
  tamaink::sleep_gesture::Controller g;
  using Event = tamaink::sleep_gesture::Controller::Event;
  assert(g.update(0, true, true) == Event::None);
  assert(g.update(1999, true, true) == Event::None);
  assert(g.update(1999, false, true) == Event::Cancelled);
  assert(g.update(3000, false, false) == Event::None);
  tamaink::sleep_gesture::Controller boundary;
  assert(boundary.update(100, true, true) == Event::None);
  assert(boundary.update(2099, true, true) == Event::None); // 1999 ms
  assert(boundary.update(2100, false, false) == Event::Trigger); // exactly 2000 ms
  tamaink::sleep_gesture::Controller boundaryLate;
  assert(boundaryLate.update(100, true, true) == Event::None);
  assert(boundaryLate.update(2101, false, false) == Event::Trigger); // release sampled at 2001 ms
  assert(g.update(0xFFFFFF00u, true, true) == Event::None);
  assert(g.update(0x000006CFu, true, true) == Event::None); // 1999 ms across rollover
  assert(g.update(0x000006D0u, false, false) == Event::Trigger); // exactly 2000 ms
  assert(g.update(0x000006D1u, true, true) == Event::None);
  assert(g.update(0x00000F3Fu, true, true) == Event::None);
  assert(g.update(0x00000F40u, false, true) == Event::None);
  assert(g.update(0x00000F41u, true, true) == Event::None); // re-press while release-waiting
  assert(g.update(0x00000F42u, false, true) == Event::None);
  assert(g.update(0x00000F43u, false, false) == Event::Trigger);
  assert(g.update(0x00000F44u, false, false) == Event::None);
  tamaink::wake::Coordinator busy;
  assert(busy.request(true, true, true, false, false));
  assert(!busy.request(true, true, true, false, false));
  assert(!busy.eligible(true, true, true, false, false));
  return 0;
}
