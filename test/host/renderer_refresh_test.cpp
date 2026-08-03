#include "tamaink_renderer_refresh.h"
#include <cassert>
#include <cstdint>

using tamaink::render::RefreshCadence;
using tamaink::render::RefreshKind;
using tamaink::render::preserveFullRefresh;

int main() {
  assert(!preserveFullRefresh(false, false));
  assert(preserveFullRefresh(true, false));
  assert(preserveFullRefresh(false, true));
  assert(preserveFullRefresh(true, true));
  bool coalescedForceFull = true;
  coalescedForceFull = preserveFullRefresh(coalescedForceFull, false);
  assert(coalescedForceFull); // a newer fast frame cannot downgrade an in-flight full refresh
  RefreshCadence cadence;
  assert(cadence.next() == RefreshKind::Full);
  cadence.presented(RefreshKind::Full);
  assert(cadence.fastFrames() == 0);
  // A rendered/rejected snapshot that is never presented does not advance.
  assert(cadence.next() == RefreshKind::Fast);
  assert(cadence.fastFrames() == 0);
  for (int i = 0; i < 64; ++i) {
    assert(cadence.next() == RefreshKind::Fast);
    cadence.presented(RefreshKind::Fast);
  }
  assert(cadence.fastFrames() == 64);
  assert(cadence.next() == RefreshKind::Full);
  cadence.presented(RefreshKind::Full);
  assert(cadence.fastFrames() == 0);
  for (int cycle = 0; cycle < 3; ++cycle) {
    for (int i = 0; i < 64; ++i) {
      assert(cadence.next() == RefreshKind::Fast);
      cadence.presented(RefreshKind::Fast);
    }
    assert(cadence.next() == RefreshKind::Full);
    cadence.presented(RefreshKind::Full);
  }
  // Rejected/unpresented frames do not advance the cadence.
  assert(cadence.next() == RefreshKind::Fast);
  cadence.presented(RefreshKind::Fast);
  assert(cadence.fastFrames() == 1);
  // Saturation protects against overflow and keeps promotion deterministic.
  for (int i = 0; i < 255; ++i) cadence.presented(RefreshKind::Fast);
  assert(cadence.fastFrames() == 64);
  assert(cadence.next() == RefreshKind::Full);
  for (unsigned threshold : {32u, 64u, 128u}) {
    RefreshCadence runtime(static_cast<std::uint8_t>(threshold));
    runtime.presented(RefreshKind::Full);
    for (unsigned i = 0; i < threshold; ++i) {
      assert(runtime.next() == RefreshKind::Fast);
      runtime.presented(RefreshKind::Fast);
    }
    assert(runtime.fastFrames() == threshold);
    assert(runtime.next() == RefreshKind::Full);
  }
  return 0;
}
