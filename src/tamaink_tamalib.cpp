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
void no_frequency(u32_t) {}
void no_play(bool_t) {}
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
  }
  out->timestamp = g_timestamp;
  std::memcpy(out->lcd, g_lcd, sizeof(g_lcd));
  out->icons = g_icons;
  out->button_a = g_buttons[0]; out->button_b = g_buttons[1]; out->button_c = g_buttons[2];
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

}  // namespace tamaink::tamalib
#endif  // TAMAINK_HOST_TAMALIB
