#include "tamaink_emulator_state.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace tamaink::emulator;
// Payload offsets mirror write_payload's field order (all values are bytes).
constexpr std::size_t kNextPcOffset = 2;
constexpr std::size_t kInterruptTriggeredOffset = 2 + 2 + 2 + 2 + 5 + 4 + 32 + 4 + 3 + 4 + 2;
constexpr std::size_t kInputPort0Offset = kInterruptTriggeredOffset + 22 + 1 + kMemorySize;
constexpr std::size_t kCpuTimestampFrequencyOffset = kInputPort0Offset + 2;
constexpr std::size_t kTamalibTimestampFrequencyOffset = kCpuTimestampFrequencyOffset + 16 + 11;
constexpr std::size_t kFramerateOffset = kTamalibTimestampFrequencyOffset + 4;
constexpr std::size_t kSoundEnabledOffset = kFramerateOffset + 1 + 8;
static std::uint32_t crc(const std::uint8_t *p, std::size_t n) {
  std::uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (int i = 0; i < 8; ++i)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
  }
  return ~c;
}
static void repair_crc(std::uint8_t *blob) {
  const auto value = crc(blob, kEncodedSize - 4);
  for (int i = 0; i < 4; ++i) blob[kEncodedSize - 4 + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
static bool eq(const State &a, const State &b) {
  if (a.pc != b.pc || a.next_pc != b.next_pc || a.x != b.x || a.y != b.y || a.a != b.a || a.b != b.b ||
      a.np != b.np || a.sp != b.sp || a.flags != b.flags ||
      a.tick_counter != b.tick_counter ||
      a.program_timer_timestamp != b.program_timer_timestamp ||
      a.program_timer_enabled != b.program_timer_enabled ||
      a.program_timer_data != b.program_timer_data ||
      a.program_timer_reload != b.program_timer_reload ||
      a.call_depth != b.call_depth || a.cpu_halted != b.cpu_halted ||
      a.cpu_timestamp_frequency != b.cpu_timestamp_frequency ||
      a.reference_timestamp != b.reference_timestamp ||
      a.cpu_frequency != b.cpu_frequency ||
      a.scaled_cycle_accumulator != b.scaled_cycle_accumulator ||
      a.speed_ratio != b.speed_ratio ||
      a.previous_cycles != b.previous_cycles ||
      a.execution_mode != b.execution_mode ||
      a.execution_step_depth != b.execution_step_depth ||
      a.screen_timestamp != b.screen_timestamp ||
      a.tamalib_timestamp_frequency != b.tamalib_timestamp_frequency ||
      a.framerate != b.framerate ||
      a.virtual_timestamp != b.virtual_timestamp ||
      a.sound_frequency != b.sound_frequency ||
      a.sound_enabled != b.sound_enabled || a.icons != b.icons || a.buttons != b.buttons)
    return false;
  for (int i = 0; i < 8; ++i)
    if (a.clock_timer_timestamps[i] != b.clock_timer_timestamps[i])
      return false;
  for (int i = 0; i < 6; ++i)
    if (a.interrupts[i].factor != b.interrupts[i].factor ||
        a.interrupts[i].mask != b.interrupts[i].mask ||
        a.interrupts[i].triggered != b.interrupts[i].triggered ||
        a.interrupts[i].vector != b.interrupts[i].vector)
      return false;
  for (std::size_t i = 0; i < kMemorySize; ++i)
    if (a.memory[i] != b.memory[i])
      return false;
  for (int i = 0; i < 16; ++i) if (a.lcd[i] != b.lcd[i]) return false;
  return a.input_port_states[0] == b.input_port_states[0] && a.input_port_states[1] == b.input_port_states[1];
}
int main() {
  State s{};
  s.pc = 0x1234;
  s.next_pc = 0x1235;
  s.x = 0x234;
  s.y = 0x345;
  s.a = 1;
  s.b = 2;
  s.np = 3;
  s.sp = 0xaa;
  s.flags = 5;
  s.tick_counter = 0x10203040;
  for (int i = 0; i < 8; i++)
    s.clock_timer_timestamps[i] = 0x100 + i;
  s.program_timer_timestamp = 0xabcdef01;
  s.program_timer_enabled = 1;
  s.program_timer_data = 0x77;
  s.program_timer_reload = 0x88;
  s.call_depth = 0xfedcba98;
  for (int i = 0; i < 6; i++)
    s.interrupts[i] = {(std::uint8_t)(i + 1), (std::uint8_t)(i + 2),
                       (std::uint8_t)(i & 1), (std::uint8_t)(0xa0 + i)};
  s.cpu_halted = 1;
  for (std::size_t i = 0; i < kMemorySize; i++)
    s.memory[i] = (std::uint8_t)(i * 37);
  s.input_port_states[0] = 0xa;
  s.input_port_states[1] = 3;
  s.cpu_timestamp_frequency = 100;
  s.reference_timestamp = 200;
  s.cpu_frequency = 300;
  s.scaled_cycle_accumulator = 400;
  s.speed_ratio = 0xfe;
  s.previous_cycles = 12;
  s.execution_mode = 5;
  s.execution_step_depth = 500;
  s.screen_timestamp = 600;
  s.tamalib_timestamp_frequency = 700;
  s.framerate = 60;
  s.virtual_timestamp = 800;
  s.sound_frequency = 900;
  s.sound_enabled = 1; s.buttons = 5; s.icons = 3; s.lcd[0] = 0x1234;
  const std::uint8_t rom[8] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87};
  std::uint8_t a[kEncodedSize], b[kEncodedSize];
  assert(encode(s, a, sizeof a, rom) == kEncodedSize);
  assert(encode(s, b, sizeof b, rom) == kEncodedSize &&
         std::memcmp(a, b, sizeof a) == 0);
  assert(a[0] == 'T' && a[1] == 'I' && a[2] == 'S' && a[3] == '1');
  assert(a[4] == 2 && a[5] == 0 && a[6] == 0x46 && a[7] == 0xe0);
  for (int i = 0; i < 8; i++)
    assert(a[8 + i] == rom[i]);
  assert(a[16] == (kPayloadSize & 0xff) && a[17] == (kPayloadSize >> 8));
  assert(a[20] == (kEncodedSize & 0xff) && a[21] == (kEncodedSize >> 8));
  assert(a[24] == 32 && a[25] == 0 && a[26] == 0 && a[27] == 0 && a[28] == 0 &&
         a[29] == 0 && a[30] == 0 && a[31] == 0);
  assert(crc(a, kEncodedSize - 4) != 0);
  State out{};
  assert(decode(a, sizeof a, rom, &out) == DecodeError::None && eq(s, out));
  assert(encode(s, nullptr, kEncodedSize, rom) == 0 &&
         encode(s, a, kEncodedSize - 1, rom) == 0 &&
         encode(s, a, kEncodedSize, nullptr) == 0);
  auto expect_encode_invalid = [&](auto mutate) {
    State invalid = s;
    mutate(invalid);
    assert(encode(invalid, a, sizeof a, rom) == 0);
  };
  expect_encode_invalid([](State& v) { v.pc = 0x2000; });
  expect_encode_invalid([](State& v) { v.a = 0x10; });
  expect_encode_invalid([](State& v) { v.program_timer_enabled = 2; });
  expect_encode_invalid([](State& v) { v.cpu_timestamp_frequency = 0; });
  expect_encode_invalid([](State& v) { v.execution_mode = 6; });
  State sentinel{};
  sentinel.pc = 77;
  assert(decode(nullptr, 0, rom, &sentinel) == DecodeError::Null &&
         sentinel.pc == 77);
  assert(decode(a, 10, rom, &sentinel) == DecodeError::Truncated &&
         sentinel.pc == 77);
  assert(decode(a, kEncodedSize + 1, rom, &sentinel) == DecodeError::Trailing &&
         sentinel.pc == 77);
  assert(decode(a, sizeof a, nullptr, &sentinel) == DecodeError::Null &&
         decode(a, sizeof a, rom, nullptr) == DecodeError::Null);
  std::uint8_t m[kEncodedSize];
  // Malformed v2 payloads have a repaired CRC so they reach field validation.
  auto expect_invalid_payload = [&](std::size_t offset, std::uint8_t value) {
    std::memcpy(m, a, sizeof m);
    m[kHeaderSize + offset] = value;
    repair_crc(m);
    State unchanged{}; unchanged.pc = 0xface; unchanged.next_pc = 0xbeef;
    unchanged.interrupts[0].triggered = 1; unchanged.input_port_states[0] = 0xa;
    unchanged.buttons = 7; unchanged.cpu_frequency = 0x12345678;
    assert(decode(m, sizeof m, rom, &unchanged) == DecodeError::Invalid);
    assert(unchanged.pc == 0xface && unchanged.next_pc == 0xbeef &&
           unchanged.interrupts[0].triggered == 1 && unchanged.input_port_states[0] == 0xa &&
           unchanged.buttons == 7 && unchanged.cpu_frequency == 0x12345678);
  };
  auto expect_invalid_zero_u32 = [&](std::size_t offset) {
    std::memcpy(m, a, sizeof m);
    std::memset(m + kHeaderSize + offset, 0, sizeof(std::uint32_t));
    repair_crc(m);
    State unchanged{}; unchanged.pc = 0xface; unchanged.next_pc = 0xbeef;
    unchanged.interrupts[0].triggered = 1; unchanged.input_port_states[0] = 0xa;
    unchanged.buttons = 7; unchanged.cpu_frequency = 0x12345678;
    assert(decode(m, sizeof m, rom, &unchanged) == DecodeError::Invalid);
    assert(unchanged.pc == 0xface && unchanged.next_pc == 0xbeef &&
           unchanged.interrupts[0].triggered == 1 && unchanged.input_port_states[0] == 0xa &&
           unchanged.buttons == 7 && unchanged.cpu_frequency == 0x12345678);
  };
  std::memcpy(m, a, sizeof m); m[kHeaderSize + kNextPcOffset] = 0; m[kHeaderSize + kNextPcOffset + 1] = 0x20;
  repair_crc(m);
  State unchanged{}; unchanged.pc = 0xface; unchanged.next_pc = 0xbeef;
  unchanged.interrupts[0].triggered = 1; unchanged.input_port_states[0] = 0xa;
  unchanged.buttons = 7; unchanged.cpu_frequency = 0x12345678;
  assert(decode(m, sizeof m, rom, &unchanged) == DecodeError::Invalid &&
         unchanged.pc == 0xface && unchanged.next_pc == 0xbeef &&
         unchanged.interrupts[0].triggered == 1 && unchanged.input_port_states[0] == 0xa &&
         unchanged.buttons == 7 && unchanged.cpu_frequency == 0x12345678);
  expect_invalid_payload(kInterruptTriggeredOffset, 2);
  expect_invalid_payload(kInputPort0Offset, 0x10);
  expect_invalid_zero_u32(kCpuTimestampFrequencyOffset);
  expect_invalid_zero_u32(kTamalibTimestampFrequencyOffset);
  expect_invalid_payload(kFramerateOffset, 0);
  expect_invalid_payload(kSoundEnabledOffset, 2);

  // Field-level validation is exercised above through typed mutations.
  std::memcpy(m, a, sizeof m);
  m[0] ^= 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Magic && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[4] = 3;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Version && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[6] ^= 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Compatibility && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[12] ^= 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Rom && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[16] = 0;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Length && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[20] = 0;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Length &&
         sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[24] = 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Header && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[26] = 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Header && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[28] = 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Header && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[100] ^= 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Crc && sentinel.pc == 77);
  std::printf("state codec size=%zu crc=%08x\n", kEncodedSize,
              crc(a, kEncodedSize - 4));
}
