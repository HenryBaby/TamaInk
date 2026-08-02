#include "tamaink_clock.h"
namespace tamaink::clock {
void ClockState::reset(std::uint32_t nowUs, std::uint32_t ticks) { fast_ = false; reanchor(nowUs, ticks); }
void ClockState::reanchor(std::uint32_t nowUs, std::uint32_t ticks) { anchorUs_ = nowUs; anchorTicks_ = ticks; timestamp_ = ticks; }
void ClockState::enterFast() { fast_ = true; }
bool ClockState::exitFast(std::uint32_t nowUs) { if (!fast_) return false; reanchor(nowUs, timestamp_); fast_ = false; return true; }
std::uint32_t ClockState::now(std::uint32_t nowUs) const {
  if (fast_) return timestamp_;
  const std::uint32_t elapsedUs = nowUs - anchorUs_;
  const std::uint64_t delta = (static_cast<std::uint64_t>(elapsedUs) * 32768u) / 1000000u;
  return anchorTicks_ + static_cast<std::uint32_t>(delta);
}
bool ClockState::fastAdvanceTo(std::uint32_t deadline) { if (!fast_) return false; timestamp_ = deadline; return true; }
} // namespace tamaink::clock
