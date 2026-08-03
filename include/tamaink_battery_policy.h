#pragma once

namespace tamaink::battery {
enum class Backend { X3Gauge, X4Adc, Unknown };
enum class Safety { AutomaticProtection, TelemetryOnly, Disabled };
struct Policy { Backend backend; Safety safety; const char* message; constexpr bool automaticProtection() const { return safety == Safety::AutomaticProtection; } };
constexpr Policy policy(Backend backend) {
  switch (backend) {
    case Backend::X3Gauge: return {backend, Safety::AutomaticProtection, "X3 BQ27220 gauge; automatic low-battery protection enabled"};
    case Backend::X4Adc: return {backend, Safety::TelemetryOnly, "X4 ADC battery telemetry; automatic low-battery protection disabled pending validation"};
    default: return {Backend::Unknown, Safety::Disabled, "Unknown battery backend; automatic low-battery protection disabled"};
  }
}
}
