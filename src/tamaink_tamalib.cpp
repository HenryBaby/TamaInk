#include "tamaink_tamalib.h"

#if defined(TAMAINK_HOST_TAMALIB)
#include <cstring>
#include <type_traits>

extern "C" {
#include "tamalib.h"
}

#if !defined(E0C6S46_SUPPORT) || defined(E0C6S48_SUPPORT)
#error "TamaInk adapter requires E0C6S46 only"
#endif
static_assert(MEM_RAM_SIZE == 0x280, "E0C6S46 RAM size mismatch");
static_assert(MEM_DISPLAY1_SIZE == 0x050, "E0C6S46 display size mismatch");
static_assert(std::is_same_v<u12_t, std::uint16_t>,
              "u12_t must match uint16_t");

namespace tamaink::tamalib {
namespace {
bool g_initialized_once = false;
Adapter* g_active = nullptr;
std::uint32_t g_timestamp = 0;
std::uint32_t g_lcd[16] = {};
std::uint8_t g_icons = 0;
bool g_buttons[3] = {};
std::uint32_t g_sound_frequency = 0;
bool g_sound_enabled = false;

void* no_malloc(u32_t) { return nullptr; }
void no_free(void*) {}
void no_void() {}
bool_t no_log_enabled(log_level_t) { return 0; }
void no_log(log_level_t, char*, ...) {}
void sleep_until(timestamp_t ts) { g_timestamp = ts; }
timestamp_t get_timestamp() { return g_timestamp; }
void no_screen() {}
void set_lcd_matrix(u8_t x, u8_t y, bool_t val) {
  if (x < 32 && y < 16) {
    const std::uint32_t bit = std::uint32_t{1} << x;
    if (val) g_lcd[y] |= bit; else g_lcd[y] &= ~bit;
  }
}
void set_lcd_icon(u8_t icon, bool_t val) {
  if (icon < 8) {
    const std::uint8_t bit = static_cast<std::uint8_t>(1u << icon);
    if (val) g_icons |= bit; else g_icons &= static_cast<std::uint8_t>(~bit);
  }
}
void no_frequency(u32_t hz) { g_sound_frequency = hz; }
void no_play(bool_t on) { g_sound_enabled = on != 0; }
int no_handler() { return 0; }

hal_t g_hal = {no_malloc, no_free, no_void, no_log_enabled, no_log,
               sleep_until, get_timestamp, no_screen, set_lcd_matrix,
               set_lcd_icon, no_frequency, no_play, no_handler};

void fill_snapshot(Snapshot* out) {
  if (!out) return;
  *out = Snapshot{};
  state_t* s = cpu_get_state();
  if (s) {
    out->pc = *s->pc; out->tick_counter = *s->tick_counter;
    out->x = *s->x; out->y = *s->y; out->a = *s->a; out->b = *s->b;
    out->np = *s->np; out->sp = *s->sp; out->flags = *s->flags;
    out->button_interrupt_factor =
        s->interrupts[INT_K00_K03_SLOT].factor_flag_reg;
  }
  out->timestamp = g_timestamp;
  std::memcpy(out->lcd, g_lcd, sizeof(g_lcd));
  out->icons = g_icons;
  out->button_a = g_buttons[0]; out->button_b = g_buttons[1]; out->button_c = g_buttons[2];
}

bool valid_state(const emulator::State& s) {
  return s.pc <= 0x1fff && s.next_pc <= 0x1fff && s.x <= 0xfff && s.y <= 0xfff &&
         s.a <= 0xf && s.b <= 0xf && s.np <= 0x1f && s.flags <= 0xf &&
         s.program_timer_enabled <= 1 && s.cpu_halted <= 1 && s.previous_cycles <= 12 &&
         s.execution_mode <= 5 && s.sound_enabled <= 1 && s.buttons <= 7 &&
         s.cpu_timestamp_frequency && s.cpu_frequency && s.tamalib_timestamp_frequency &&
         s.framerate && s.input_port_states[0] <= 0xf && s.input_port_states[1] <= 0xf &&
         s.interrupts[0].factor <= 0xf && s.interrupts[0].mask <= 0xf && s.interrupts[0].triggered <= 1 &&
         s.interrupts[1].factor <= 0xf && s.interrupts[1].mask <= 0xf && s.interrupts[1].triggered <= 1 &&
         s.interrupts[2].factor <= 0xf && s.interrupts[2].mask <= 0xf && s.interrupts[2].triggered <= 1 &&
         s.interrupts[3].factor <= 0xf && s.interrupts[3].mask <= 0xf && s.interrupts[3].triggered <= 1 &&
         s.interrupts[4].factor <= 0xf && s.interrupts[4].mask <= 0xf && s.interrupts[4].triggered <= 1 &&
         s.interrupts[5].factor <= 0xf && s.interrupts[5].mask <= 0xf && s.interrupts[5].triggered <= 1;
}
}

Adapter::~Adapter() { (void)release(); }

Status Adapter::init(const std::uint16_t* program, std::size_t count,
                     Snapshot* out) {
  if (!program || count != kProgramWords) return Status::InvalidInput;
  if (g_initialized_once || g_active) return Status::AlreadyInitialized;
  g_initialized_once = true;
  g_active = this;
  g_timestamp = 0; std::memset(g_lcd, 0, sizeof(g_lcd)); g_icons = 0; std::memset(g_buttons, 0, sizeof(g_buttons));
  tamalib_register_hal(&g_hal);
  if (tamalib_init(reinterpret_cast<const u12_t*>(program), nullptr, 32768u) != 0) {
    g_active = nullptr; tamalib_release(); return Status::InitFailure;
  }
  tamalib_set_exec_mode(EXEC_MODE_RUN);
  fill_snapshot(out);
  return Status::Ok;
}

Status Adapter::release() {
  if (g_active != this) return Status::NotInitialized;
  tamalib_release();
  g_active = nullptr;
  return Status::Ok;
}

Status Adapter::step(std::size_t count, Snapshot* out) {
  if (g_active != this) return Status::NotInitialized;
  for (std::size_t i = 0; i < count; ++i) tamalib_step();
  fill_snapshot(out);
  return Status::Ok;
}

Status Adapter::set_button(Button button, bool pressed) {
  if (g_active != this) return Status::NotInitialized;
  const unsigned index = static_cast<unsigned>(button);
  if (index > 2) return Status::InvalidInput;
  g_buttons[index] = pressed;
  tamalib_set_button(static_cast<button_t>(index), pressed ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
  return Status::Ok;
}

Status Adapter::snapshot(Snapshot* out) const {
  if (g_active != this) return Status::NotInitialized;
  if (!out) return Status::InvalidInput;
  fill_snapshot(out); return Status::Ok;
}

Status Adapter::export_state(emulator::State* out) const {
  if (g_active != this) return Status::NotInitialized;
  if (!out) return Status::InvalidInput;
  tamalib_extended_state_t ext{};
  tamalib_export_extended_state(&ext);
  emulator::State tmp{}; auto& c = ext.cpu;
  tmp.pc = c.pc; tmp.next_pc = c.next_pc; tmp.x = c.x; tmp.y = c.y;
  tmp.a = c.a; tmp.b = c.b; tmp.np = c.np; tmp.sp = c.sp; tmp.flags = c.flags;
  tmp.tick_counter = c.tick_counter;
  std::memcpy(tmp.clock_timer_timestamps, c.clk_timer_timestamps, sizeof tmp.clock_timer_timestamps);
  tmp.program_timer_timestamp = c.prog_timer_timestamp;
  tmp.program_timer_enabled = c.prog_timer_enabled; tmp.program_timer_data = c.prog_timer_data;
  tmp.program_timer_reload = c.prog_timer_rld; tmp.call_depth = c.call_depth;
  for (int i = 0; i < 6; ++i) { tmp.interrupts[i].factor = c.interrupts[i].factor_flag_reg; tmp.interrupts[i].mask = c.interrupts[i].mask_reg; tmp.interrupts[i].triggered = c.interrupts[i].triggered; tmp.interrupts[i].vector = c.interrupts[i].vector; }
  tmp.cpu_halted = c.cpu_halted; std::memcpy(tmp.memory, c.memory, sizeof tmp.memory);
  tmp.input_port_states[0] = c.input_states[0]; tmp.input_port_states[1] = c.input_states[1];
  tmp.cpu_timestamp_frequency = c.ts_freq; tmp.reference_timestamp = c.ref_ts;
  tmp.cpu_frequency = c.cpu_frequency; tmp.scaled_cycle_accumulator = c.scaled_cycle_accumulator;
  tmp.speed_ratio = c.speed_ratio; tmp.previous_cycles = c.previous_cycles;
  tmp.execution_mode = static_cast<std::uint8_t>(ext.exec_mode); tmp.execution_step_depth = ext.step_depth;
  tmp.screen_timestamp = ext.screen_ts; tmp.tamalib_timestamp_frequency = ext.ts_freq;
  tmp.framerate = ext.framerate; tmp.virtual_timestamp = g_timestamp;
  tmp.sound_frequency = g_sound_frequency; tmp.sound_enabled = g_sound_enabled;
  std::memcpy(tmp.lcd, g_lcd, sizeof tmp.lcd);
  tmp.icons = g_icons; tmp.buttons = (g_buttons[0] ? 1 : 0) | (g_buttons[1] ? 2 : 0) | (g_buttons[2] ? 4 : 0);
  *out = tmp;
  return Status::Ok;
}

Status Adapter::import_state(const emulator::State& s) {
  if (g_active != this) return Status::NotInitialized;
  if (!valid_state(s)) return Status::InvalidInput;
  tamalib_extended_state_t ext{};
  auto& c = ext.cpu;
  c.pc=s.pc; c.next_pc=s.next_pc; c.x=s.x; c.y=s.y; c.a=s.a; c.b=s.b; c.np=s.np; c.sp=s.sp; c.flags=s.flags;
  c.tick_counter=s.tick_counter; std::memcpy(c.clk_timer_timestamps,s.clock_timer_timestamps,sizeof c.clk_timer_timestamps);
  c.prog_timer_timestamp=s.program_timer_timestamp; c.prog_timer_enabled=s.program_timer_enabled; c.prog_timer_data=s.program_timer_data; c.prog_timer_rld=s.program_timer_reload; c.call_depth=s.call_depth;
  for(int i=0;i<6;++i){ c.interrupts[i].factor_flag_reg=s.interrupts[i].factor; c.interrupts[i].mask_reg=s.interrupts[i].mask; c.interrupts[i].triggered=s.interrupts[i].triggered; c.interrupts[i].vector=s.interrupts[i].vector; }
  c.cpu_halted=s.cpu_halted; std::memcpy(c.memory,s.memory,sizeof c.memory); c.input_states[0]=s.input_port_states[0]; c.input_states[1]=s.input_port_states[1]; c.ts_freq=s.cpu_timestamp_frequency; c.ref_ts=s.reference_timestamp; c.cpu_frequency=s.cpu_frequency; c.scaled_cycle_accumulator=s.scaled_cycle_accumulator; c.speed_ratio=s.speed_ratio; c.previous_cycles=s.previous_cycles;
  ext.exec_mode=static_cast<exec_mode_t>(s.execution_mode); ext.step_depth=s.execution_step_depth; ext.screen_ts=s.screen_timestamp; ext.ts_freq=s.tamalib_timestamp_frequency; ext.framerate=s.framerate;
  if (!tamalib_import_extended_state(&ext)) return Status::InvalidInput;
  g_timestamp=s.virtual_timestamp; std::memcpy(g_lcd,s.lcd,sizeof g_lcd); g_icons=s.icons;
  g_sound_frequency = s.sound_frequency; g_sound_enabled = s.sound_enabled != 0;
  for(int i=0;i<3;++i) g_buttons[i]=(s.buttons & (1u<<i)) != 0;
  return Status::Ok;
}

}  // namespace tamaink::tamalib
#endif  // TAMAINK_HOST_TAMALIB
