#pragma once
#include <cstdint>

namespace tamaink::sleep_gesture {

// POWER hold gesture: a short release emits Tap; a release after the threshold
// emits Trigger. Unsigned elapsed time keeps the threshold rollover-safe.
class Controller {
 public:
  static constexpr uint32_t kHoldMs = 2000;
  static constexpr uint32_t kTapPulseMs = 100;
  enum class Event : uint8_t { None, Tap, Trigger };

  Event update(uint32_t now, bool powerPressed) {
    if (!active_) {
      if (powerPressed) {
        active_ = true;
        startedAt_ = now;
      }
      return Event::None;
    }
    if (powerPressed) return Event::None;
    const bool reachedThreshold = static_cast<uint32_t>(now - startedAt_) >= kHoldMs;
    active_ = false;
    return reachedThreshold ? Event::Trigger : Event::Tap;
  }

  void reset() {
    active_ = false;
    startedAt_ = 0;
  }
  bool active() const { return active_; }

 private:
  bool active_ = false;
  uint32_t startedAt_ = 0;
};

constexpr bool tapPulseElapsed(uint32_t startedAt, uint32_t now) {
  return static_cast<uint32_t>(now - startedAt) >= Controller::kTapPulseMs;
}

}  // namespace tamaink::sleep_gesture
