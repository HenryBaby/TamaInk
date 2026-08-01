#include <cassert>
#include <cstdio>
#include <vector>
#include "tamaink_tamalib.h"

int main() {
  using namespace tamaink::tamalib;
  std::vector<std::uint16_t> program(kProgramWords, 0x0FFBu);
  Adapter adapter;
  Snapshot s{};
  assert(adapter.init(nullptr, kProgramWords, &s) == Status::InvalidInput);
  assert(adapter.step(1, &s) == Status::NotInitialized);
  assert(adapter.init(program.data(), program.size() - 1, &s) == Status::InvalidInput);
  assert(adapter.init(program.data(), program.size(), &s) == Status::Ok);
  assert(adapter.snapshot(nullptr) == Status::InvalidInput);
  Adapter conflict;
  assert(conflict.init(program.data(), program.size(), nullptr) == Status::AlreadyInitialized);
  assert(s.pc == 0x100);
  assert(adapter.set_button(Button::A, true) == Status::Ok);
  assert(adapter.set_button(Button::A, false) == Status::Ok);
  assert(adapter.set_button(Button::B, true) == Status::Ok);
  assert(adapter.set_button(Button::B, false) == Status::Ok);
  assert(adapter.set_button(Button::C, true) == Status::Ok);
  assert(adapter.set_button(Button::C, false) == Status::Ok);
  assert(adapter.set_button(static_cast<Button>(99), true) == Status::InvalidInput);
  assert(adapter.step(64, &s) == Status::Ok);
  assert(s.pc == 0x140 && s.tick_counter == 315 && s.timestamp == 315);
  assert(s.icons == 0);
  assert(!s.button_a && !s.button_b && !s.button_c);
  for (auto row : s.lcd) assert(row == 0);
  std::printf("pc=%04X tick=%u ts=%u icons=%u lcd0=%08X\n", s.pc,
              s.tick_counter, s.timestamp, s.icons, s.lcd[0]);
  assert(adapter.release() == Status::Ok);
  assert(adapter.release() == Status::NotInitialized);
  return 0;
}
