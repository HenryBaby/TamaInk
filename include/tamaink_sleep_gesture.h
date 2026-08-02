#pragma once
#include <cstdint>

namespace tamaink::sleep_gesture {

// A BACK+POWER chord qualifies only after the threshold while both remain held,
// and emits one event on the first frame where both are released.
class Controller {
 public:
  static constexpr uint32_t kHoldMs = 2000;
  enum class Event : uint8_t { None, Cancelled, Trigger };

  Event update(uint32_t now, bool backPressed, bool powerPressed) {
    if (!active_) {
      if (backPressed && powerPressed) {
        active_ = true;
        qualified_ = false;
        startedAt_ = now;
      }
      return Event::None;
    }
    if (qualified_) {
      if (!backPressed && !powerPressed) {
        active_ = false;
        qualified_ = false;
        return Event::Trigger;
      }
      // Once qualified, tolerate staggered release (and re-press) until both
      // buttons are finally released; this still emits exactly one trigger.
      return Event::None;
    }
    if (backPressed && powerPressed) {
      if (static_cast<uint32_t>(now - startedAt_) >= kHoldMs) qualified_ = true;
      return Event::None;
    }
    const bool releasedBoth = !backPressed && !powerPressed;
    const bool reachedThreshold = static_cast<uint32_t>(now - startedAt_) >= kHoldMs;
    active_ = false;
    qualified_ = false;
    // If both release between polls, the elapsed duration still proves a
    // continuous chord; a single-button release before qualification cancels.
    return (releasedBoth && reachedThreshold) ? Event::Trigger : Event::Cancelled;
  }

  void reset() { active_ = false; qualified_ = false; startedAt_ = 0; }
  bool active() const { return active_; }
  bool qualified() const { return qualified_; }

 private:
  bool active_ = false;
  bool qualified_ = false;
  uint32_t startedAt_ = 0;
};

}  // namespace tamaink::sleep_gesture
