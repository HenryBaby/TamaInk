#pragma once

#include <cstddef>
#include <cstdint>
#include "tamaink_emulator_state.h"

namespace tamaink::tamalib {

constexpr std::size_t kProgramWords = 6144;

enum class Status : std::uint8_t {
  Ok = 0, InvalidInput, AlreadyInitialized, NotInitialized, InitFailure
};

enum class Button : std::uint8_t { A = 0, B, C };

struct Snapshot {
  std::uint16_t pc = 0;
  std::uint32_t tick_counter = 0;
  std::uint32_t timestamp = 0;
  std::uint16_t x = 0, y = 0;
  std::uint8_t a = 0, b = 0, np = 0, sp = 0, flags = 0;
  std::uint8_t button_interrupt_factor = 0;
  std::uint32_t lcd[16]{};
  std::uint8_t icons = 0;
  bool button_a = false, button_b = false, button_c = false;
};

class Adapter {
 public:
  // TamaLib uses process-global state (including cpu_step's static cycle
  // accumulator); only one successful initialization is permitted per process.
  // The program buffer is caller-owned and must outlive the active adapter.
  Adapter() = default;
  ~Adapter();
  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  Status init(const std::uint16_t* program, std::size_t count,
              Snapshot* snapshot = nullptr);
  Status release();
  Status step(std::size_t count, Snapshot* snapshot = nullptr);
  Status set_button(Button button, bool pressed);
  Status snapshot(Snapshot* out) const;
  Status export_state(emulator::State* out) const;
  Status import_state(const emulator::State& state);
};

}  // namespace tamaink::tamalib
