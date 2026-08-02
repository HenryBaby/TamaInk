#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "tamaink_emulator_state.h"
#include "tamaink_tamalib.h"

namespace {
using tamaink::emulator::State;
using tamaink::tamalib::Snapshot;

bool same_snapshot(const Snapshot& a, const Snapshot& b) {
  return a.pc == b.pc && a.tick_counter == b.tick_counter &&
         a.timestamp == b.timestamp && a.x == b.x && a.y == b.y &&
         a.a == b.a && a.b == b.b && a.np == b.np && a.sp == b.sp &&
         a.flags == b.flags &&
         a.button_interrupt_factor == b.button_interrupt_factor &&
         std::memcmp(a.lcd, b.lcd, sizeof a.lcd) == 0 &&
         a.icons == b.icons && a.button_a == b.button_a &&
         a.button_b == b.button_b && a.button_c == b.button_c;
}

void run_continuation(tamaink::tamalib::Adapter& adapter, Snapshot* out) {
  using tamaink::tamalib::Button;
  assert(adapter.step(17) == tamaink::tamalib::Status::Ok);
  assert(adapter.set_button(Button::B, true) == tamaink::tamalib::Status::Ok);
  assert(adapter.step(31) == tamaink::tamalib::Status::Ok);
  assert(adapter.set_button(Button::A, false) == tamaink::tamalib::Status::Ok);
  assert(adapter.set_button(Button::C, true) == tamaink::tamalib::Status::Ok);
  assert(adapter.step(23, out) == tamaink::tamalib::Status::Ok);
}
}  // namespace

int main() {
  using namespace tamaink::emulator;
  using namespace tamaink::tamalib;

  // A full-size synthetic ROM keeps this test independent of external ROM data.
  std::vector<std::uint16_t> rom(kProgramWords, 0x0FFBu);
  const std::uint8_t rom_id[8] = {0x54, 0x41, 0x4d, 0x41, 0x53, 0x34, 0x36, 0x01};
  Adapter adapter;
  Snapshot initial{};
  assert(adapter.init(rom.data(), rom.size(), &initial) == Status::Ok);
  assert(adapter.set_button(Button::A, true) == Status::Ok);
  assert(adapter.step(100) == Status::Ok);
  assert(adapter.set_button(Button::C, true) == Status::Ok);
  assert(adapter.step(37) == Status::Ok);

  State saved{};
  assert(adapter.export_state(&saved) == Status::Ok);
  std::vector<std::uint8_t> encoded(encoded_size());
  assert(encode(saved, encoded.data(), encoded.size(), rom_id) == encoded.size());
  State decoded{};
  assert(decode(encoded.data(), encoded.size(), rom_id, &decoded) == DecodeError::None);

  Snapshot expected_snapshot{};
  run_continuation(adapter, &expected_snapshot);
  State expected_state{};
  assert(adapter.export_state(&expected_state) == Status::Ok);
  std::vector<std::uint8_t> expected_bytes(encoded_size());
  assert(encode(expected_state, expected_bytes.data(), expected_bytes.size(), rom_id) ==
         expected_bytes.size());

  assert(adapter.import_state(decoded) == Status::Ok);
  Snapshot restored_snapshot{};
  run_continuation(adapter, &restored_snapshot);
  State restored_state{};
  assert(adapter.export_state(&restored_state) == Status::Ok);
  std::vector<std::uint8_t> restored_bytes(encoded_size());
  assert(encode(restored_state, restored_bytes.data(), restored_bytes.size(), rom_id) ==
         restored_bytes.size());

  assert(std::memcmp(expected_bytes.data(), restored_bytes.data(), expected_bytes.size()) == 0);
  assert(same_snapshot(expected_snapshot, restored_snapshot));
  std::printf("continuation-equivalence state=%zu pc=%04x tick=%u buttons=%u\n",
              expected_bytes.size(), restored_snapshot.pc,
              restored_snapshot.tick_counter,
              static_cast<unsigned>(restored_snapshot.button_a) |
                  (static_cast<unsigned>(restored_snapshot.button_b) << 1) |
                  (static_cast<unsigned>(restored_snapshot.button_c) << 2));
  assert(adapter.release() == Status::Ok);
}
