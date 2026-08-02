#pragma once

#include <cstdint>

namespace tamaink::wake {

// Catch-up is deliberately bounded until a later gate can execute it safely.
constexpr std::uint32_t kMaxPlannedSeconds = 300;
constexpr std::uint32_t kMaxInstructionAttempts = 2000000;

struct CatchupPlan {
  std::uint64_t requestedSeconds{};
  std::uint32_t plannedSeconds{};
  bool capped{};
  std::uint32_t targetVirtualTimestamp{};
  std::uint32_t maxInstructionAttempts{};
  bool available{};
};

// Computes a plan only; it never mutates emulator state or executes steps.
// Timestamp arithmetic is modular uint32; deltas above INT32_MAX are rejected
// because their forward direction would be ambiguous.
// Later execution must step until targetVirtualTimestamp or maxInstructionAttempts,
// whichever comes first. Instruction attempts are not virtual ticks.
CatchupPlan makeCatchupPlan(std::uint64_t elapsedSeconds,
                            std::uint32_t virtualTimestamp,
                            std::uint32_t timestampFrequency);

} // namespace tamaink::wake
