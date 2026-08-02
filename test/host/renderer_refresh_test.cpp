#include "tamaink_renderer_refresh.h"
#include <cassert>

using tamaink::render::RefreshCadence;
using tamaink::render::RefreshKind;

int main() {
  RefreshCadence cadence;
  assert(cadence.next() == RefreshKind::Full);
  cadence.presented(RefreshKind::Full);
  assert(cadence.fastFrames() == 0);
  // A rendered/rejected snapshot that is never presented does not advance.
  assert(cadence.next() == RefreshKind::Fast);
  assert(cadence.fastFrames() == 0);
  for (int i = 0; i < 8; ++i) {
    assert(cadence.next() == RefreshKind::Fast);
    cadence.presented(RefreshKind::Fast);
  }
  assert(cadence.fastFrames() == 8);
  assert(cadence.next() == RefreshKind::Full);
  cadence.presented(RefreshKind::Full);
  assert(cadence.fastFrames() == 0);
  for (int cycle = 0; cycle < 3; ++cycle) {
    for (int i = 0; i < 8; ++i) {
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
  assert(cadence.fastFrames() == 8);
  assert(cadence.next() == RefreshKind::Full);
  return 0;
}
