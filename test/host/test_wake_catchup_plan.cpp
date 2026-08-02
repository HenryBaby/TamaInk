#include "tamaink_wake_catchup_plan.h"
#include <cassert>
#include <cstdint>
#include <limits>

using tamaink::wake::makeCatchupPlan;
using tamaink::wake::kMaxInstructionAttempts;
using tamaink::wake::kMaxPlannedSeconds;

int main() {
  auto zero = makeCatchupPlan(0, 123, 32768);
  assert(zero.available && zero.plannedSeconds == 0 && !zero.capped && zero.targetVirtualTimestamp == 123);
  auto normal = makeCatchupPlan(2, 100, 1000);
  assert(normal.available && normal.requestedSeconds == 2 && normal.plannedSeconds == 2 && normal.targetVirtualTimestamp == 2100);
  auto exact = makeCatchupPlan(300, 0, 32768);
  assert(exact.available && exact.plannedSeconds == kMaxPlannedSeconds && !exact.capped && exact.targetVirtualTimestamp == 9830400);
  auto capped = makeCatchupPlan(301, 0, 1);
  assert(capped.available && capped.capped && capped.plannedSeconds == kMaxPlannedSeconds);
  assert(!makeCatchupPlan(1, 0, 0).available);
  assert(!makeCatchupPlan(1, 0, std::numeric_limits<std::uint32_t>::max()).available);
  auto wraps = makeCatchupPlan(1, std::numeric_limits<std::uint32_t>::max(), 1);
  assert(wraps.available && wraps.targetVirtualTimestamp == 0);
  auto nearBoundary = makeCatchupPlan(1, std::numeric_limits<std::uint32_t>::max() - 1, 1);
  assert(nearBoundary.available && nearBoundary.targetVirtualTimestamp == std::numeric_limits<std::uint32_t>::max());
  auto halfRange = makeCatchupPlan(1, 0, std::numeric_limits<std::int32_t>::max());
  assert(halfRange.available && halfRange.targetVirtualTimestamp == std::numeric_limits<std::int32_t>::max());
  auto ambiguous = makeCatchupPlan(2, 0, std::numeric_limits<std::int32_t>::max());
  assert(!ambiguous.available);
  auto immutable = makeCatchupPlan(7, 77, 11);
  assert(immutable.requestedSeconds == 7 && immutable.targetVirtualTimestamp == 154);
  assert(immutable.maxInstructionAttempts == kMaxInstructionAttempts);
  return 0;
}
