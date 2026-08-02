#pragma once

#include <cstddef>
#include <cstdint>

namespace tamaink::emulator {

// Fixed canonical blob format. Memory is pinned S46 LOW_FOOTPRINT.
constexpr std::size_t kMemorySize = 464;
constexpr std::size_t kHeaderSize = 32;
constexpr std::size_t kPayloadSize = 658;
constexpr std::size_t kEncodedSize = 694;
constexpr std::uint16_t kFormatVersion = 2;
constexpr std::uint16_t kCompatibilityId = 0xE046;

struct InterruptRecord {
  std::uint8_t factor{}, mask{}, triggered{}, vector{};
};
struct State {
  std::uint16_t pc{}, next_pc{}, x{}, y{};
  std::uint8_t a{}, b{}, np{}, sp{}, flags{};
  std::uint32_t tick_counter{}, clock_timer_timestamps[8]{},
      program_timer_timestamp{};
  std::uint8_t program_timer_enabled{}, program_timer_data{},
      program_timer_reload{};
  std::uint32_t call_depth{};
  InterruptRecord interrupts[6]{};
  std::uint8_t cpu_halted{};
  std::uint8_t memory[kMemorySize]{}, input_port_states[2]{};
  std::uint32_t cpu_timestamp_frequency{}, reference_timestamp{},
      cpu_frequency{}, scaled_cycle_accumulator{};
  std::uint8_t speed_ratio{}, previous_cycles{}, execution_mode{};
  std::uint32_t execution_step_depth{}, screen_timestamp{},
      tamalib_timestamp_frequency{};
  std::uint8_t framerate{};
  std::uint32_t virtual_timestamp{}, sound_frequency{};
  std::uint8_t sound_enabled{};
  std::uint32_t lcd[16]{};
  std::uint8_t icons{}, buttons{};
};

static_assert(2 + 6 + 5 + 4 + 32 + 4 + 3 + 4 + 24 + 1 + 464 + 2 + 16 + 3 + 12 + 1 +
                  4 + 4 + 1 + 64 + 2 ==
              kPayloadSize);
static_assert(kHeaderSize + kPayloadSize + 4 == kEncodedSize);

enum class DecodeError : std::uint8_t {
  None,
  Null,
  Truncated,
  Trailing,
  Magic,
  Version,
  Header,
  Compatibility,
  Rom,
  Length,
  Crc,
  Invalid
};

std::size_t encoded_size();
std::size_t encode(const State &, std::uint8_t *, std::size_t,
                   const std::uint8_t rom[8]);
// On failure, output remains unchanged. The host adapter provides a live bridge.
DecodeError decode(const std::uint8_t *, std::size_t, const std::uint8_t rom[8],
                   State *);

} // namespace tamaink::emulator
