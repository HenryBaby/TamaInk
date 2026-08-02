#pragma once

#include <cstdint>

namespace tamaink::battery {

enum class WarningState : uint8_t { Unknown, Normal, Low };

struct UpdateResult {
  WarningState state;
  bool sampleValid;
  bool changed;
};

class WarningPolicy {
 public:
  WarningPolicy() = default;
  WarningState state() const { return state_; }
  UpdateResult update(bool percentageKnown, uint16_t percentage);

 private:
  WarningState state_ = WarningState::Unknown;
};

constexpr uint32_t kTelemetryIntervalMs = 60UL * 1000UL;

// Unsigned subtraction remains correct across millis() wrap-around.
constexpr bool deadlineDue(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

class Schedule {
 public:
  bool due(uint32_t now) const { return armed_ && deadlineDue(now, deadline_); }
  void arm(uint32_t now) { deadline_ = now + kTelemetryIntervalMs; armed_ = true; }
  void disarm() { armed_ = false; }
  uint32_t deadline() const { return deadline_; }

 private:
  uint32_t deadline_ = 0;
  bool armed_ = false;
};

}  // namespace tamaink::battery
