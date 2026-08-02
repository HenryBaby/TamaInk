#include "tamaink_low_battery_sleep.h"
#include <cassert>
using tamaink::low_battery::Coordinator;
using tamaink::low_battery::Sample;
using tamaink::low_battery::Owner;
using tamaink::low_battery::TransactionArbiter;
static Sample low() { return {true, 15, true, false}; }
int main() {
  Coordinator c;
  assert(!c.observe(low(), true)); assert(c.consecutive() == 1);
  assert(c.observe(low(), true)); assert(c.pending());
  assert(!c.observe(low(), true));
  c.saveSucceeded(); assert(c.latched()); assert(!c.observe(low(), true));
  Coordinator x; assert(!x.observe(low(), false)); assert(!x.observe(low(), false));
  assert(!x.observe(low(), true)); assert(x.consecutive() == 1);
  assert(x.observe(low(), true));
  Coordinator y; assert(!y.observe({false, 0, true, false}, true)); assert(y.consecutive() == 0);
  assert(!y.observe({true, 15, false, false}, true)); assert(y.consecutive() == 0);
  assert(!y.observe({true, 15, true, true}, true)); assert(y.consecutive() == 0);
  assert(!y.observe({true, 16, true, false}, true)); assert(y.consecutive() == 0);
  assert(!y.observe({true, 20, true, false}, true)); assert(y.consecutive() == 0);
  assert(!y.observe({true, 16, false, false}, true)); assert(y.consecutive() == 0);
  assert(!y.observe({true, 20, false, false}, true)); assert(y.consecutive() == 0);
  assert(!y.observe(low(), true)); assert(y.consecutive() == 1);
  assert(!y.observe({true, 101, true, false}, true)); assert(y.consecutive() == 0);
  assert(!y.observe(low(), true)); assert(y.observe(low(), true));
  y.saveFailed(); assert(!y.pending()); assert(!y.latched());
  Coordinator z; assert(!z.observe(low(), true)); assert(!z.observe({false, 0, false, false}, true)); assert(!z.observe(low(), true));
  assert(!z.observe(low(), false)); assert(z.consecutive() == 0); assert(!z.observe(low(), true)); assert(z.consecutive() == 1);
  assert(!z.observe({true, 101, true, false}, true)); assert(z.consecutive() == 0);
  TransactionArbiter a; assert(a.claim(Owner::LowBattery)); assert(!a.claim(Owner::Manual)); assert(!a.complete(Owner::Manual)); assert(a.owner() == Owner::LowBattery); assert(a.complete(Owner::LowBattery)); assert(a.owner() == Owner::None); assert(a.claim(Owner::Automatic)); assert(!a.complete(Owner::LowBattery)); assert(a.fail(Owner::Automatic)); assert(a.claim(Owner::WakeDiagnostic)); assert(!a.claim(Owner::LowBattery)); assert(a.fail(Owner::WakeDiagnostic)); assert(a.claim(Owner::Manual)); assert(a.complete(Owner::Manual));
  return 0;
}
