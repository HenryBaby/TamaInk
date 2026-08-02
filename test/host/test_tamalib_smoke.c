#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include "tamalib.h"

/* P1 must use the S46 memory map; fail loudly if upstream defaults to S48. */
#if defined(E0C6S48_SUPPORT) || !defined(E0C6S46_SUPPORT)
#error "TamaLib smoke test must select E0C6S46, not E0C6S48"
#endif
_Static_assert(MEM_RAM_SIZE == 0x280, "E0C6S46 RAM size expected");
_Static_assert(MEM_DISPLAY1_SIZE == 0x050, "E0C6S46 display size expected");

static void *host_malloc(u32_t size) { return malloc(size); }
static void host_free(void *ptr) { free(ptr); }
static void host_halt(void) {}
static bool_t host_log_enabled(log_level_t level) { (void)level; return 0; }
static void host_log(log_level_t level, char *buff, ...)
{
	(void)level;
	(void)buff;
}
static void host_sleep_until(timestamp_t ts) { (void)ts; }
static timestamp_t host_get_timestamp(void) { return 0; }
static void host_update_screen(void) {}
static void host_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
	(void)x;
	(void)y;
	(void)val;
}
static void host_set_lcd_icon(u8_t icon, bool_t val)
{
	(void)icon;
	(void)val;
}
static void host_set_frequency(u32_t freq) { (void)freq; }
static void host_play_frequency(bool_t en) { (void)en; }
static int host_handler(void) { return 1; }

static hal_t host_hal = {
	.malloc = host_malloc,
	.free = host_free,
	.halt = host_halt,
	.is_log_enabled = host_log_enabled,
	.log = host_log,
	.sleep_until = host_sleep_until,
	.get_timestamp = host_get_timestamp,
	.update_screen = host_update_screen,
	.set_lcd_matrix = host_set_lcd_matrix,
	.set_lcd_icon = host_set_lcd_icon,
	.set_frequency = host_set_frequency,
	.play_frequency = host_play_frequency,
	.handler = host_handler,
};

int main(void)
{
	static const u12_t null_program[1] = {0};
	tamalib_register_hal(&host_hal);
	assert(tamalib_init(null_program, NULL, 32768u) == 0);
	tamalib_release();
	return 0;
}
