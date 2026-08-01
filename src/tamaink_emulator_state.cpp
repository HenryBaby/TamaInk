#include "tamaink_emulator_state.h"

#include <cstring>

namespace tamaink::emulator {
namespace {
constexpr std::uint8_t kMagic[4] = {'T', 'I', 'S', '1'};
struct Writer {
  std::uint8_t *p;
  void u8(std::uint8_t v) { *p++ = v; }
  void u16(std::uint16_t v) {
    u8(v);
    u8(v >> 8);
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      u8(static_cast<std::uint8_t>(v >> (8 * i)));
  }
};
struct Reader {
  const std::uint8_t *p;
  const std::uint8_t *e;
  bool ok = true;
  std::uint8_t u8() {
    if (p >= e) {
      ok = false;
      return 0;
    }
    return *p++;
  }
  std::uint16_t u16() {
    const auto a = u8(), b = u8();
    return std::uint16_t(a) | (std::uint16_t(b) << 8);
  }
  std::uint32_t u32() {
    const auto a = u16(), b = u16();
    return std::uint32_t(a) | (std::uint32_t(b) << 16);
  }
};
std::uint32_t crc(const std::uint8_t *p, std::size_t n) {
  std::uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (int i = 0; i < 8; ++i)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
  }
  return ~c;
}
bool valid(const State &s) {
  if (s.pc > 0x1fff || s.x > 0xfff || s.y > 0xfff || s.a > 0xf || s.b > 0xf ||
      s.np > 0x1f || s.flags > 0xf || s.program_timer_enabled > 1 ||
      s.cpu_halted > 1 || s.previous_cycles > 12 || s.execution_mode > 5 ||
      s.sound_enabled > 1 || !s.cpu_timestamp_frequency || !s.cpu_frequency ||
      !s.tamalib_timestamp_frequency || !s.framerate)
    return false;
  for (const auto &i : s.interrupts)
    if (i.factor > 0xf || i.mask > 0xf || i.triggered > 1)
      return false;
  for (const auto v : s.input_port_states)
    if (v > 0xf)
      return false;
  return true;
}
void write_payload(Writer &w, const State &s) {
  w.u16(s.pc);
  w.u16(s.x);
  w.u16(s.y);
  for (auto v : {s.a, s.b, s.np, s.sp, s.flags})
    w.u8(v);
  w.u32(s.tick_counter);
  for (auto v : s.clock_timer_timestamps)
    w.u32(v);
  w.u32(s.program_timer_timestamp);
  for (auto v :
       {s.program_timer_enabled, s.program_timer_data, s.program_timer_reload})
    w.u8(v);
  w.u32(s.call_depth);
  for (const auto &i : s.interrupts) {
    w.u8(i.factor);
    w.u8(i.mask);
    w.u8(i.triggered);
    w.u8(i.vector);
  }
  w.u8(s.cpu_halted);
  for (auto v : s.memory)
    w.u8(v);
  for (auto v : s.input_port_states)
    w.u8(v);
  for (auto v : {s.cpu_timestamp_frequency, s.reference_timestamp,
                 s.cpu_frequency, s.scaled_cycle_accumulator})
    w.u32(v);
  for (auto v : {s.speed_ratio, s.previous_cycles, s.execution_mode})
    w.u8(v);
  for (auto v : {s.execution_step_depth, s.screen_timestamp,
                 s.tamalib_timestamp_frequency})
    w.u32(v);
  w.u8(s.framerate);
  w.u32(s.virtual_timestamp);
  w.u32(s.sound_frequency);
  w.u8(s.sound_enabled);
}
void read_payload(Reader &r, State &s) {
  s.pc = r.u16();
  s.x = r.u16();
  s.y = r.u16();
  s.a = r.u8();
  s.b = r.u8();
  s.np = r.u8();
  s.sp = r.u8();
  s.flags = r.u8();
  s.tick_counter = r.u32();
  for (auto &v : s.clock_timer_timestamps)
    v = r.u32();
  s.program_timer_timestamp = r.u32();
  s.program_timer_enabled = r.u8();
  s.program_timer_data = r.u8();
  s.program_timer_reload = r.u8();
  s.call_depth = r.u32();
  for (auto &i : s.interrupts) {
    i.factor = r.u8();
    i.mask = r.u8();
    i.triggered = r.u8();
    i.vector = r.u8();
  }
  s.cpu_halted = r.u8();
  for (auto &v : s.memory)
    v = r.u8();
  for (auto &v : s.input_port_states)
    v = r.u8();
  s.cpu_timestamp_frequency = r.u32();
  s.reference_timestamp = r.u32();
  s.cpu_frequency = r.u32();
  s.scaled_cycle_accumulator = r.u32();
  s.speed_ratio = r.u8();
  s.previous_cycles = r.u8();
  s.execution_mode = r.u8();
  s.execution_step_depth = r.u32();
  s.screen_timestamp = r.u32();
  s.tamalib_timestamp_frequency = r.u32();
  s.framerate = r.u8();
  s.virtual_timestamp = r.u32();
  s.sound_frequency = r.u32();
  s.sound_enabled = r.u8();
}
} // namespace
std::size_t encoded_size() { return kEncodedSize; }
std::size_t encode(const State &s, std::uint8_t *out, std::size_t cap,
                   const std::uint8_t rom[8]) {
  if (!out || !rom || cap < kEncodedSize || !valid(s))
    return 0;
  Writer w{out};
  for (auto v : kMagic)
    w.u8(v);
  w.u16(kFormatVersion);
  w.u16(kCompatibilityId);
  for (int i = 0; i < 8; ++i)
    w.u8(rom[i]);
  w.u32(kPayloadSize);
  w.u32(kEncodedSize);
  w.u16(kHeaderSize);
  w.u16(0);
  w.u32(0);
  write_payload(w, s);
  Writer c{out + kHeaderSize + kPayloadSize};
  c.u32(crc(out, kHeaderSize + kPayloadSize));
  return kEncodedSize;
}
DecodeError decode(const std::uint8_t *d, std::size_t n,
                   const std::uint8_t rom[8], State *out) {
  if (!d || !rom || !out)
    return DecodeError::Null;
  if (n < kEncodedSize)
    return DecodeError::Truncated;
  if (n > kEncodedSize)
    return DecodeError::Trailing;
  if (std::memcmp(d, kMagic, 4) != 0)
    return DecodeError::Magic;
  Reader h{d + 4, d + kHeaderSize};
  if (h.u16() != kFormatVersion)
    return DecodeError::Version;
  if (h.u16() != kCompatibilityId)
    return DecodeError::Compatibility;
  for (int i = 0; i < 8; ++i)
    if (h.u8() != rom[i])
      return DecodeError::Rom;
  if (h.u32() != kPayloadSize || h.u32() != kEncodedSize)
    return DecodeError::Length;
  if (h.u16() != kHeaderSize || h.u16() != 0 || h.u32() != 0)
    return DecodeError::Header;
  Reader t{d + kHeaderSize + kPayloadSize, d + kEncodedSize};
  if (t.u32() != crc(d, kHeaderSize + kPayloadSize))
    return DecodeError::Crc;
  State tmp{};
  Reader r{d + kHeaderSize, d + kHeaderSize + kPayloadSize};
  read_payload(r, tmp);
  if (!r.ok || r.p != r.e || !valid(tmp))
    return DecodeError::Invalid;
  *out = tmp;
  return DecodeError::None;
}
} // namespace tamaink::emulator
