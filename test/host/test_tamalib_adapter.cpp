#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
#include "tamalib.h"
}
#include "tamaink_tamalib.h"
#include "tamaink_emulator_state.h"

int main() {
  using namespace tamaink::tamalib;
  std::vector<std::uint16_t> program(kProgramWords, 0x0FFBu);
  Adapter adapter;
  Snapshot s{};
  assert(adapter.init(nullptr, kProgramWords, &s) == Status::InvalidInput);
  assert(adapter.step(1, &s) == Status::NotInitialized);
  assert(adapter.init(program.data(), program.size() - 1, &s) == Status::InvalidInput);
  assert(adapter.init(program.data(), program.size(), &s) == Status::Ok);
  assert(s.button_interrupt_factor == 0);
  assert(adapter.snapshot(nullptr) == Status::InvalidInput);
  Adapter conflict;
  assert(conflict.init(program.data(), program.size(), nullptr) == Status::AlreadyInitialized);
  assert(s.pc == 0x100);
  assert(adapter.set_button(Button::A, true) == Status::Ok);
  assert(adapter.snapshot(&s) == Status::Ok && s.button_interrupt_factor == 0x4);
  assert(adapter.set_button(Button::B, true) == Status::Ok);
  assert(adapter.snapshot(&s) == Status::Ok && s.button_interrupt_factor == 0x6);
  assert(adapter.set_button(Button::C, true) == Status::Ok);
  assert(adapter.snapshot(&s) == Status::Ok && s.button_interrupt_factor == 0x7);
  assert(adapter.set_button(Button::A, false) == Status::Ok);
  assert(adapter.set_button(Button::B, false) == Status::Ok);
  assert(adapter.set_button(Button::C, false) == Status::Ok);
  assert(adapter.snapshot(&s) == Status::Ok && s.button_interrupt_factor == 0x7);
  assert(adapter.set_button(static_cast<Button>(99), true) == Status::InvalidInput);
  assert(adapter.step(64, &s) == Status::Ok);
  tamaink::emulator::State saved{};
  assert(adapter.export_state(&saved) == Status::Ok);
  const std::uint8_t rom_id[8] = {0}; std::uint8_t blob[tamaink::emulator::kEncodedSize];
  assert(tamaink::emulator::encode(saved, blob, sizeof blob, rom_id) == sizeof blob);
  tamaink::emulator::State decoded{};
  assert(tamaink::emulator::decode(blob, sizeof blob, rom_id, &decoded) == tamaink::emulator::DecodeError::None);
  auto assert_import_atomic = [&](auto mutate) {
    tamaink::emulator::State invalid = decoded; mutate(invalid);
    assert(adapter.import_state(invalid) == Status::InvalidInput);
    tamaink::emulator::State after{}; std::uint8_t after_blob[ tamaink::emulator::kEncodedSize ];
    assert(adapter.export_state(&after) == Status::Ok);
    assert(tamaink::emulator::encode(after, after_blob, sizeof after_blob, rom_id) == sizeof after_blob);
    assert(std::memcmp(blob, after_blob, sizeof blob) == 0);
  };
  assert_import_atomic([](auto& v) { v.interrupts[0].triggered = 2; });
  assert_import_atomic([](auto& v) { v.input_port_states[0] = 0x10; });
  assert_import_atomic([](auto& v) { v.program_timer_enabled = 2; });
  assert_import_atomic([](auto& v) { v.cpu_frequency = 0; });
  auto sound_state = decoded; sound_state.sound_frequency = 440; sound_state.sound_enabled = 1;
  assert(adapter.import_state(sound_state) == Status::Ok);
  tamaink::emulator::State sound_export{};
  assert(adapter.export_state(&sound_export) == Status::Ok && sound_export.sound_frequency == 440 && sound_export.sound_enabled == 1);
  assert(adapter.import_state(decoded) == Status::Ok); // restore original cache before continuation
  assert(s.pc == 0x140 && s.tick_counter == 315 && s.timestamp == 315);
  assert(s.icons == 0);
  assert(!s.button_a && !s.button_b && !s.button_c);
  for (auto row : s.lcd) assert(row == 0);
  assert(s.button_interrupt_factor == 0x7);

  // Test-only synthetic display-memory injection. This exercises TamaLib's
  // refresh/HAL mapping, not ROM execution and adds no adapter API.
  state_t* state = cpu_get_state();
  SET_DISP1_MEMORY(state->memory, 0xE00, 1);
  SET_DISP1_MEMORY(state->memory, 0xE10, 1);
  tamalib_refresh_hw();
  assert(adapter.snapshot(&s) == Status::Ok);
  assert((s.lcd[0] & 1u) != 0 && (s.icons & 1u) != 0);
  assert((s.lcd[1] & 1u) == 0);
  std::printf("pc=%04X tick=%u ts=%u event=%u icons=%u lcd0=%08X\n", s.pc,
              s.tick_counter, s.timestamp, s.button_interrupt_factor, s.icons,
              s.lcd[0]);
  assert(adapter.release() == Status::Ok);
  assert(adapter.release() == Status::NotInitialized);
  return 0;
}
