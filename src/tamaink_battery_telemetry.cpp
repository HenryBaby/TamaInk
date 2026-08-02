#include "tamaink_battery_telemetry.h"

namespace tamaink::battery {

UpdateResult WarningPolicy::update(bool percentageKnown, uint16_t percentage) {
  const WarningState before = state_;
  if (!percentageKnown || percentage > 100) return {state_, false, false};

  if (state_ == WarningState::Low) {
    if (percentage >= 20) state_ = WarningState::Normal;
  } else if (percentage <= 15) {
    state_ = WarningState::Low;
  } else if (state_ == WarningState::Unknown) {
    state_ = WarningState::Normal;
  }
  return {state_, true, state_ != before};
}

}  // namespace tamaink::battery
