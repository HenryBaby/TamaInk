#include "tamaink_renderer_dispatch.h"
#include <cassert>

int main() {
  tamaink::render::FrameDispatch d;
  assert(!d.eligible(0));
  d.changed(false, 0);
  assert(d.eligible(0));
  d.queued(0);
  d.changed(false, 1000);
  d.changed(true, 1100);  // Coalesced icon update remains urgent.
  assert(d.eligible(1100));
  d.queued(1100);
  d.changed(false, 1200);
  assert(!d.eligible(1200));
  assert(d.eligible(2600));
  d.queued(2600);
  d.changed(true, 1501);
  assert(d.eligible(1501));
  d.queued(1501);
  d.changed(false, 0xfffffff0u);
  d.queued(0xfffffff0u);
  d.changed(false, 0x000005d0u);
  assert(d.eligible(0x000005d0u));
  d.reset();
  assert(!d.pending() && !d.eligible(0));
  return 0;
}
