#include "tamaink_wake_catchup.h"
#include <algorithm>

namespace tamaink::wake {
CatchupController::CatchupController(std::uint32_t target, std::uint32_t maxAttempts,
                                     std::uint32_t initial)
    : maxAttempts_(maxAttempts) {
  result_.targetTimestamp = target;
  result_.finalTimestamp = initial;
  done_ = reached(initial);
  if (done_) result_.outcome = CatchupOutcome::Reached;
}

bool CatchupController::reached(std::uint32_t current) const {
  return static_cast<std::int32_t>(result_.targetTimestamp - current) <= 0;
}

std::size_t CatchupController::nextBatch(std::uint32_t current) {
  if (done_) return 0;
  if (reached(current)) { stop(CatchupOutcome::Reached, current); return 0; }
  const std::uint32_t remaining = maxAttempts_ - result_.attempts;
  if (remaining == 0) { stop(CatchupOutcome::InstructionLimit, current); return 0; }
  return std::min<std::uint32_t>(64u, remaining);
}

void CatchupController::observe(std::uint32_t current, std::size_t attempted, bool adapterOk) {
  if (done_) return;
  const std::uint32_t count = static_cast<std::uint32_t>(attempted);
  result_.attempts += count > (maxAttempts_ - result_.attempts) ? (maxAttempts_ - result_.attempts) : count;
  result_.finalTimestamp = current;
  if (!adapterOk) { done_ = true; result_.outcome = CatchupOutcome::AdapterError; return; }
  if (reached(current)) { done_ = true; result_.outcome = CatchupOutcome::Reached; return; }
  if (result_.attempts >= maxAttempts_) { done_ = true; result_.outcome = CatchupOutcome::InstructionLimit; }
}

void CatchupController::stop(CatchupOutcome outcome, std::uint32_t current) {
  if (done_) return;
  done_ = true; result_.outcome = outcome; result_.finalTimestamp = current;
}

const char* catchupOutcomeName(CatchupOutcome outcome) {
  switch (outcome) {
    case CatchupOutcome::Reached: return "reached";
    case CatchupOutcome::InstructionLimit: return "instruction-limit";
    case CatchupOutcome::Watchdog: return "watchdog";
    case CatchupOutcome::AdapterError: return "adapter-error";
  }
  return "unknown";
}
} // namespace tamaink::wake
