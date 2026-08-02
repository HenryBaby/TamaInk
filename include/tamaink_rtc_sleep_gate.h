#pragma once
#include <cstdint>

namespace tamaink::rtc {
struct DateTime {
  std::uint16_t year;
  std::uint8_t month, day, hour, minute, second, weekday;
};
bool decodeDs3231(const std::uint8_t raw[7], DateTime &out,
                  std::uint64_t &epochSeconds);
constexpr std::uint64_t kTagMask = 0xFFFF000000000000ULL;
constexpr std::uint64_t kTag = 0xA5D3000000000000ULL;
constexpr std::uint64_t kMaxEpochSeconds = 6311433599ULL;
std::uint64_t encodeTagged(std::uint64_t epochSeconds);
bool decodeTagged(std::uint64_t value, std::uint64_t &epochSeconds);
bool elapsed(std::uint64_t saved, std::uint64_t current,
             std::uint64_t &seconds);
} // namespace tamaink::rtc
