#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    uint8_t level = 0;
    uint8_t vector = 0;

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    ng_neogeo_reset_runtime();
    ng_m68k_clear_interrupt_level();
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_m68k_set_interrupt_level(4, 28);
    CHECK(!ng_m68k_take_interrupt(4, &level, &vector));
    CHECK(ng_m68k_take_interrupt(3, &level, &vector));
    CHECK(level == 4);
    CHECK(vector == 28);
    CHECK(!ng_m68k_take_interrupt(4, &level, &vector));

    ng_m68k_clear_interrupt_level();
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_m68k_set_interrupt_level(7, 31);
    CHECK(ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(level == 7);
    CHECK(vector == 31);
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(ng_m68k_take_interrupt(6, &level, &vector));
    CHECK(level == 7);
    CHECK(vector == 31);

    ng_m68k_set_interrupt_level(6, 30);
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    ng_m68k_set_interrupt_level(7, 31);
    CHECK(ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(level == 7);
    CHECK(vector == 31);

    ng_m68k_clear_interrupt_level();
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_vblank_interrupt();
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    CHECK(!ng_m68k_take_interrupt(1, &level, &vector));
    ng_neogeo_ack_interrupts(NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_vblank_interrupt();
    ng_neogeo_request_timer_interrupt();
    CHECK(ng_m68k_take_interrupt(1, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng_neogeo_ack_interrupts(NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng_neogeo_ack_interrupts(NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_reset_interrupt();
    CHECK(ng_m68k_take_interrupt(2, &level, &vector));
    CHECK(level == 3);
    CHECK(vector == 27);
    ng68k_write16(0x003C000Cu, NG_NEO_IRQACK_RESET);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_vblank_interrupt();
    ng_neogeo_request_timer_interrupt();
    ng68k_write16(0x003C000Cu, NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write8(0x003C000Du, NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_lspc_mode() == 0);
    CHECK(ng_neogeo_timer_reload() == 0);
    CHECK(ng_neogeo_timer_counter() == 0);
    ng68k_write16(NG_NEO_REG_TIMERHIGH, 0x1234u);
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x5678u);
    CHECK(ng_neogeo_timer_reload() == 0x12345678u);
    CHECK(ng_neogeo_timer_counter() == 0);
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE);
    CHECK(ng_neogeo_lspc_mode() == (NG_NEO_LSPCMODE_TIMER_ENABLE |
                                    NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE));
    ng68k_write16(NG_NEO_REG_TIMERHIGH, 0x0000u);
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0004u);
    CHECK(ng_neogeo_timer_reload() == 0x00000004u);
    CHECK(ng_neogeo_timer_counter() == 0x00000004u);
    ng_neogeo_advance_timer(4);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));
    ng_neogeo_advance_timer(1);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_ZERO);
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0001u);
    CHECK(ng_neogeo_timer_counter() == 1u);
    ng_neogeo_advance_timer(2);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    CHECK(ng_neogeo_timer_counter() == 1u);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    ng_neogeo_advance_timer(1);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));
    ng_neogeo_advance_timer(1);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0002u);
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME);
    ng_neogeo_begin_vblank();
    CHECK(ng_neogeo_timer_counter() == 2u);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng_neogeo_advance_timer(3);
    CHECK(ng_m68k_take_interrupt(1, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0000u);
    ng_neogeo_advance_timer(16);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));
    ng68k_write16(NG_NEO_REG_TIMERSTOP, 0x0001u);
    CHECK(ng_neogeo_timer_stop() == 0x0001u);

    ng_neogeo_reset_runtime();
    ng68k_write8(NG_NEO_REG_LSPCMODE + 1u,
                 NG_NEO_LSPCMODE_TIMER_ENABLE |
                 NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE);
    CHECK(ng_neogeo_lspc_mode() == 0x3030u);
    ng68k_write8(NG_NEO_REG_TIMERSTOP + 1u, 0x01u);
    CHECK(ng_neogeo_timer_stop() == 0x0101u);

    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_current_scanline() == 0);
    ng_neogeo_advance_scanline();
    CHECK(ng_neogeo_current_scanline() == 1);
    ng_neogeo_advance_frame();
    CHECK(ng_neogeo_current_scanline() == 1);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);

    ng_neogeo_reset_runtime();
    for (uint32_t i = 0; i < NG_NEO_NTSC_SCANLINES_PER_FRAME; ++i) {
        ng_neogeo_advance_scanline();
    }
    CHECK(ng_neogeo_current_scanline() == 0);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_TIMERLOW,
                  (uint16_t)(NG_NEO_NTSC_PIXELS_PER_SCANLINE - 1u));
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME);
    ng_neogeo_advance_frame();
    CHECK(ng_m68k_take_interrupt(1, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);

    return 0;
}
