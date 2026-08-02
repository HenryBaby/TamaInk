#pragma once
#include <cstdint>

namespace tamaink::clock {
class ClockState {
 public:
  void reset(std::uint32_t nowUs, std::uint32_t ticks);
  void reanchor(std::uint32_t nowUs, std::uint32_t ticks);
  void enterFast();
  bool exitFast(std::uint32_t nowUs);
  std::uint32_t now(std::uint32_t nowUs) const;
  bool fastAdvanceTo(std::uint32_t deadline);
  bool fast() const { return fast_; }
  std::uint32_t timestamp() const { return timestamp_; }
 private:
  std::uint32_t timestamp_ = 0, anchorUs_ = 0, anchorTicks_ = 0;
  bool fast_ = false;
};
} // namespace tamaink::clock
