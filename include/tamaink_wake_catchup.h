#pragma once

#include <cstddef>
#include <cstdint>

namespace tamaink::wake {

enum class CatchupOutcome : std::uint8_t { Reached, InstructionLimit, Watchdog, AdapterError };

struct CatchupResult {
  CatchupOutcome outcome = CatchupOutcome::Reached;
  std::uint32_t attempts = 0;
  std::uint32_t finalTimestamp = 0;
  std::uint32_t targetTimestamp = 0;
};

// Pure execution controller. The caller performs Adapter::step and feeds back
// the resulting timestamp/status; modular signed deltas determine completion.
class CatchupController {
 public:
  CatchupController(std::uint32_t targetTimestamp, std::uint32_t maxAttempts,
                    std::uint32_t initialTimestamp);
  bool done() const { return done_; }
  std::size_t nextBatch(std::uint32_t currentTimestamp);
  void observe(std::uint32_t currentTimestamp, std::size_t attempted, bool adapterOk);
  void stop(CatchupOutcome outcome, std::uint32_t currentTimestamp);
  CatchupResult result() const { return result_; }

 private:
  bool reached(std::uint32_t currentTimestamp) const;
  bool done_ = false;
  std::uint32_t maxAttempts_ = 0;
  CatchupResult result_{};
};

const char* catchupOutcomeName(CatchupOutcome outcome);

} // namespace tamaink::wake
