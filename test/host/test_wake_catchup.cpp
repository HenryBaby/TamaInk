#include "tamaink_wake_catchup.h"
#include <cassert>
#include <cstdint>
#include <string>

using namespace tamaink::wake;

int main() {
  CatchupController zero(100, 100, 100);
  assert(zero.nextBatch(100) == 0 && zero.result().outcome == CatchupOutcome::Reached);

  CatchupController normal(130, 200, 0);
  assert(normal.nextBatch(0) == 64); normal.observe(64, 64, true);
  assert(normal.nextBatch(64) == 64); normal.observe(128, 64, true);
  assert(normal.nextBatch(128) == 64); normal.observe(130, 64, true);
  assert(normal.done() && normal.result().attempts == 192);

  CatchupController overshoot(100, 200, 0);
  const auto b = overshoot.nextBatch(0); overshoot.observe(120, b, true);
  assert(overshoot.result().outcome == CatchupOutcome::Reached);

  CatchupController wrap(2, 200, UINT32_MAX - 2);
  assert(wrap.nextBatch(UINT32_MAX - 2) == 64); wrap.observe(5, 64, true);
  assert(wrap.result().outcome == CatchupOutcome::Reached);

  CatchupController cap(1000, 65, 0);
  assert(cap.nextBatch(0) == 64); cap.observe(1, 64, true);
  assert(cap.nextBatch(1) == 1); cap.observe(2, 1, true);
  assert(cap.result().outcome == CatchupOutcome::InstructionLimit && cap.result().attempts == 65);

  CatchupController error(100, 100, 0);
  error.observe(0, error.nextBatch(0), false);
  assert(error.result().outcome == CatchupOutcome::AdapterError && error.result().attempts == 64);

  CatchupController watchdog(100, 100, 0);
  watchdog.stop(CatchupOutcome::Watchdog, 7);
  assert(watchdog.result().outcome == CatchupOutcome::Watchdog && watchdog.result().finalTimestamp == 7);
  assert(catchupOutcomeName(CatchupOutcome::InstructionLimit) == std::string("instruction-limit"));
  return 0;
}
