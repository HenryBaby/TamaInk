#include "tamaink_low_battery_sleep.h"

namespace tamaink::low_battery {

bool Coordinator::observe(const Sample& sample, bool ready) {
  if (latched_ || pending_) return false;
  const bool qualifies = ready && sample.percentageKnown && sample.percentage <= 100 &&
                         sample.percentage <= 15 && sample.chargingKnown && !sample.charging;
  if (!qualifies) { consecutive_ = 0; return false; }
  if (consecutive_ < 2) ++consecutive_;
  if (consecutive_ < 2) return false;
  pending_ = true;
  return true;
}

}  // namespace tamaink::low_battery
