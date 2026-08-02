#include "tamaink_wake_catchup_plan.h"

#include <limits>

namespace tamaink::wake {
CatchupPlan makeCatchupPlan(const std::uint64_t elapsedSeconds,
                            const std::uint32_t virtualTimestamp,
                            const std::uint32_t timestampFrequency) {
  CatchupPlan plan{};
  plan.requestedSeconds = elapsedSeconds;
  plan.maxInstructionAttempts = kMaxInstructionAttempts;
  if (timestampFrequency == 0) return plan;

  plan.capped = elapsedSeconds > kMaxPlannedSeconds;
  plan.plannedSeconds = plan.capped ? kMaxPlannedSeconds
                                    : static_cast<std::uint32_t>(elapsedSeconds);
  if (static_cast<std::uint64_t>(plan.plannedSeconds) >
      std::numeric_limits<std::uint64_t>::max() / timestampFrequency) {
    return plan;
  }
  const std::uint64_t delta = static_cast<std::uint64_t>(plan.plannedSeconds) * timestampFrequency;
  if (delta > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
    return plan;
  }
  plan.targetVirtualTimestamp = static_cast<std::uint32_t>(
      virtualTimestamp + static_cast<std::uint32_t>(delta));
  plan.available = true;
  return plan;
}
} // namespace tamaink::wake
