#include <cassert>

#include "tamaink_sleep_gesture.h"
#include "tamaink_wake_diagnostic.h"

int main() {
  tamaink::wake::Coordinator coordinator;
  assert(coordinator.request(true, true, true, false, false));
  assert(!coordinator.request(true, true, true, false, false));
  assert(!coordinator.mutationAllowed(false));
  assert(coordinator.mutationAllowed(true));
  assert(coordinator.saveSucceeded());
  assert(!coordinator.saveSucceeded());
  assert(coordinator.request(true, true, true, false, false));
  coordinator.saveFailed();
  assert(coordinator.request(true, true, true, false, false));
  coordinator.armFailed();

  using Controller = tamaink::sleep_gesture::Controller;
  using Event = Controller::Event;
  using tamaink::sleep_gesture::tapPulseElapsed;

  assert(!tapPulseElapsed(100, 199));
  assert(tapPulseElapsed(100, 200));
  assert(!tapPulseElapsed(0xffffffc0u, 0x00000023u));
  assert(tapPulseElapsed(0xffffffc0u, 0x00000024u));

  Controller tap;
  assert(tap.update(100, false) == Event::None);
  assert(tap.update(101, true) == Event::None);
  assert(tap.active());
  assert(tap.update(2099, true) == Event::None);
  assert(tap.update(2099, false) == Event::Tap);  // 1998 ms
  assert(!tap.active());

  Controller boundary;
  assert(boundary.update(100, true) == Event::None);
  assert(boundary.update(2099, true) == Event::None);
  assert(boundary.update(2100, false) == Event::Trigger);  // exactly 2000 ms
  assert(boundary.update(2101, false) == Event::None);

  Controller rollover;
  const std::uint32_t start = 0xffffff00u;
  assert(rollover.update(start, true) == Event::None);
  assert(rollover.update(start + 1999u, true) == Event::None);
  assert(rollover.update(start + 2000u, false) == Event::Trigger);

  Controller reset;
  assert(reset.update(0, true) == Event::None);
  reset.reset();
  assert(!reset.active());
  assert(reset.update(3000, false) == Event::None);

  tamaink::wake::Coordinator busy;
  assert(busy.request(true, true, true, false, false));
  assert(!busy.request(true, true, true, false, false));
  assert(!busy.eligible(true, true, true, false, false));
  return 0;
}
