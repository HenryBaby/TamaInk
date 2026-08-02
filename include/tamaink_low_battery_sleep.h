#pragma once
#include <cstdint>

namespace tamaink::low_battery {

struct Sample {
  bool percentageKnown = false;
  std::uint16_t percentage = 0;
  bool chargingKnown = false;
  bool charging = false;
};

class Coordinator {
 public:
  // Feed one sample. Returns true exactly when a sleep-save request should be issued.
  bool observe(const Sample& sample, bool ready);
  void saveSucceeded() { pending_ = false; latched_ = true; }
  void saveFailed() { pending_ = false; latched_ = false; consecutive_ = 0; }
  void cancelPending() { pending_ = false; }
  bool pending() const { return pending_; }
  bool latched() const { return latched_; }
  unsigned consecutive() const { return consecutive_; }

 private:
  unsigned consecutive_ = 0;
  bool pending_ = false;
  bool latched_ = false;
};

enum class Owner : std::uint8_t { None, Manual, Automatic, WakeDiagnostic, LowBattery };

class TransactionArbiter {
 public:
  bool claim(Owner owner) { if (owner_ != Owner::None || owner == Owner::None) return false; owner_ = owner; return true; }
  bool complete(Owner owner) { if (owner_ != owner) return false; owner_ = Owner::None; return true; }
  bool fail(Owner owner) { return complete(owner); }
  Owner owner() const { return owner_; }
 private:
  Owner owner_ = Owner::None;
};

}  // namespace tamaink::low_battery
