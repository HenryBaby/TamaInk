#include "tamaink_emulator_state.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace tamaink::emulator;
static std::uint32_t crc(const std::uint8_t *p, std::size_t n) {
  std::uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (int i = 0; i < 8; ++i)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
  }
  return ~c;
}
static void fix_crc(std::uint8_t *b) {
  const auto c = crc(b, kEncodedSize - 4);
  for (int i = 0; i < 4; ++i)
    b[kEncodedSize - 4 + i] = std::uint8_t(c >> (8 * i));
}
static bool eq(const State &a, const State &b) {
  if (a.pc != b.pc || a.x != b.x || a.y != b.y || a.a != b.a || a.b != b.b ||
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
      a.sound_enabled != b.sound_enabled)
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
  return a.input_port_states[0] == b.input_port_states[0] &&
         a.input_port_states[1] == b.input_port_states[1];
}
int main() {
  State s{};
  s.pc = 0x1234;
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
  s.sound_enabled = 1;
  const std::uint8_t rom[8] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87};
  std::uint8_t a[kEncodedSize], b[kEncodedSize];
  assert(encode(s, a, sizeof a, rom) == kEncodedSize);
  assert(encode(s, b, sizeof b, rom) == kEncodedSize &&
         std::memcmp(a, b, sizeof a) == 0);
  assert(a[0] == 'T' && a[1] == 'I' && a[2] == 'S' && a[3] == '1');
  assert(a[4] == 1 && a[5] == 0 && a[6] == 0x46 && a[7] == 0xe0);
  for (int i = 0; i < 8; i++)
    assert(a[8 + i] == rom[i]);
  assert(a[16] == 0x4e && a[17] == 2 && a[18] == 0 && a[19] == 0);
  assert(a[20] == 0x72 && a[21] == 2 && a[22] == 0 && a[23] == 0);
  assert(a[24] == 32 && a[25] == 0 && a[26] == 0 && a[27] == 0 && a[28] == 0 &&
         a[29] == 0 && a[30] == 0 && a[31] == 0);
  assert(a[622] == 0x3f && a[623] == 0xb4 && a[624] == 0xcf && a[625] == 0xa1);
  assert(crc(a, kEncodedSize - 4) == 0xa1cfb43fu);
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
  auto expect_invalid = [&](auto mutate) {
    std::uint8_t bytes[kEncodedSize];
    std::memcpy(bytes, a, sizeof bytes);
    mutate(bytes + kHeaderSize);
    fix_crc(bytes);
    sentinel.pc = 77;
    assert(decode(bytes, sizeof bytes, rom, &sentinel) == DecodeError::Invalid);
    assert(sentinel.pc == 77);
  };
  auto set_byte = [&](std::size_t offset, std::uint8_t value) {
    expect_invalid([&](std::uint8_t *payload) { payload[offset] = value; });
  };
  auto zero_u32 = [&](std::size_t offset) {
    expect_invalid([&](std::uint8_t *payload) {
      for (int i = 0; i < 4; ++i)
        payload[offset + i] = 0;
    });
  };

  set_byte(1, 0x20);   // PC > 0x1fff
  set_byte(3, 0x10);   // X > 0x0fff
  set_byte(5, 0x10);   // Y > 0x0fff
  set_byte(6, 0xff);   // A is not a nibble
  set_byte(7, 0xff);   // B is not a nibble
  set_byte(8, 0xff);   // NP is wider than five bits
  set_byte(10, 0xff);  // flags are not a nibble
  set_byte(51, 2);     // program timer enabled is not boolean
  set_byte(58, 0xff);  // interrupt factor is not a nibble
  set_byte(59, 0xff);  // interrupt mask is not a nibble
  set_byte(60, 2);     // interrupt triggered is not boolean
  set_byte(82, 2);     // CPU halted is not boolean
  set_byte(547, 0xff); // input port 0 is not a nibble
  set_byte(548, 0xff); // input port 1 is not a nibble
  zero_u32(549);       // CPU timestamp frequency is required
  zero_u32(557);       // CPU frequency is required
  set_byte(566, 13);   // previous instruction cycle count is out of range
  set_byte(567, 6);    // execution mode is out of range
  zero_u32(576);       // TamaLib timestamp frequency is required
  set_byte(580, 0);    // framerate is required
  set_byte(589, 2);    // sound enabled is not boolean
  std::uint8_t m[kEncodedSize];
  std::memcpy(m, a, sizeof m);
  m[0] ^= 1;
  assert(decode(m, sizeof m, rom, &sentinel) == DecodeError::Magic && sentinel.pc == 77);
  std::memcpy(m, a, sizeof m);
  m[4] = 2;
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
