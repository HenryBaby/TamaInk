#include "tamaink_battery_telemetry.h"
#include <cassert>

using tamaink::battery::Schedule;
using tamaink::battery::WarningPolicy;
using tamaink::battery::WarningState;

int main() {
  WarningPolicy p;
  assert(p.state() == WarningState::Unknown);
  auto r = p.update(true, 16); assert(r.sampleValid && r.changed && r.state == WarningState::Normal);
  r = p.update(true, 15); assert(r.sampleValid && r.changed && r.state == WarningState::Low);
  r = p.update(true, 16); assert(!r.changed && r.state == WarningState::Low);
  r = p.update(true, 19); assert(!r.changed && r.state == WarningState::Low);
  r = p.update(true, 20); assert(r.changed && r.state == WarningState::Normal);
  r = p.update(true, 15); assert(r.changed && r.state == WarningState::Low);
  r = p.update(true, 101); assert(!r.sampleValid && !r.changed && r.state == WarningState::Low);
  r = p.update(false, 0); assert(!r.sampleValid && !r.changed && r.state == WarningState::Low);

  WarningPolicy unknown;
  r = unknown.update(false, 0); assert(unknown.state() == WarningState::Unknown);
  r = unknown.update(true, 19); assert(r.state == WarningState::Normal);

  Schedule s;
  s.arm(0xFFFFFFF0u);
  assert(!s.due(0x00000020u));
  assert(s.due(0x0000EA50u));
  s.arm(100); assert(!s.due(100)); assert(s.due(100 + 60000));
}
