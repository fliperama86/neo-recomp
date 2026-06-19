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

    return 0;
}
