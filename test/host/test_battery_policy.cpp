#include "tamaink_battery_policy.h"
#include <cassert>
#include <cstring>
int main() {
  using namespace tamaink::battery;
  assert(policy(Backend::X3Gauge).safety == Safety::AutomaticProtection);
  assert(policy(Backend::X4Adc).safety == Safety::TelemetryOnly);
  assert(!policy(Backend::X4Adc).automaticProtection());
  assert(policy(Backend::Unknown).safety == Safety::Disabled);
  assert(std::strstr(policy(Backend::X4Adc).message, "disabled") != nullptr);
}
