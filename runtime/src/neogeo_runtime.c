#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>

#define NG_REG_IRQACK 0x003C000Cu

NgM68kState g_ng_m68k;

static uint8_t g_ng_m68k_interrupt_level;
static uint8_t g_ng_m68k_interrupt_vector;
static uint8_t g_ng_m68k_level7_edge;
static uint16_t g_ng_neogeo_irq_pending;

uint8_t ng68k_read8(uint32_t addr) {
    fprintf(stderr, "ng68k_read8 miss at $%06X\n", addr & 0xFFFFFFu);
    return 0xFF;
}

uint16_t ng68k_read16(uint32_t addr) {
    uint16_t hi = ng68k_read8(addr);
    uint16_t lo = ng68k_read8(addr + 1);
    return (uint16_t)((hi << 8) | lo);
}

uint32_t ng68k_read32(uint32_t addr) {
    uint32_t hi = ng68k_read16(addr);
    uint32_t lo = ng68k_read16(addr + 2);
    return (hi << 16) | lo;
}

void ng68k_write8(uint32_t addr, uint8_t value) {
    if ((addr & 0x00FFFFFFu) == (NG_REG_IRQACK + 1u)) {
        ng_neogeo_ack_interrupts(value);
        return;
    }
    fprintf(stderr, "ng68k_write8 miss at $%06X value=$%02X\n",
            addr & 0xFFFFFFu, value);
}

void ng68k_write16(uint32_t addr, uint16_t value) {
    if ((addr & 0x00FFFFFFu) == NG_REG_IRQACK) {
        ng_neogeo_ack_interrupts(value);
        return;
    }
    ng68k_write8(addr, (uint8_t)(value >> 8));
    ng68k_write8(addr + 1, (uint8_t)value);
}

void ng68k_write32(uint32_t addr, uint32_t value) {
    ng68k_write16(addr, (uint16_t)(value >> 16));
    ng68k_write16(addr + 2, (uint16_t)value);
}

void ng_call_by_address(uint32_t addr) {
    ng_log_dispatch_miss(addr);
}

void ng_log_dispatch_miss(uint32_t addr) {
    fprintf(stderr, "dispatch miss at $%06X\n", addr & 0xFFFFFFu);
}

void ng_m68k_stop_until_interrupt(uint16_t sr) {
    fprintf(stderr, "m68k STOP until interrupt sr=$%04X\n", sr);
}

void ng_m68k_set_interrupt_level(uint8_t level, uint8_t vector) {
    level = (uint8_t)(level & 7u);
    if (level == 7u && g_ng_m68k_interrupt_level < 7u) {
        g_ng_m68k_level7_edge = 1u;
    } else if (level < 7u) {
        g_ng_m68k_level7_edge = 0u;
    }
    g_ng_m68k_interrupt_level = level;
    g_ng_m68k_interrupt_vector = vector;
}

void ng_m68k_clear_interrupt_level(void) {
    g_ng_m68k_interrupt_level = 0;
    g_ng_m68k_interrupt_vector = 0;
    g_ng_m68k_level7_edge = 0;
}

static void ng_neogeo_refresh_interrupt_level(void) {
    if (g_ng_neogeo_irq_pending & NG_NEO_IRQACK_RESET) {
        ng_m68k_set_interrupt_level(3, 27);
    } else if (g_ng_neogeo_irq_pending & NG_NEO_IRQACK_TIMER) {
        ng_m68k_set_interrupt_level(2, 26);
    } else if (g_ng_neogeo_irq_pending & NG_NEO_IRQACK_VBLANK) {
        ng_m68k_set_interrupt_level(1, 25);
    } else {
        ng_m68k_clear_interrupt_level();
    }
}

void ng_neogeo_request_vblank_interrupt(void) {
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_VBLANK;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_request_timer_interrupt(void) {
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_TIMER;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_request_reset_interrupt(void) {
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_RESET;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_ack_interrupts(uint16_t ack_mask) {
    g_ng_neogeo_irq_pending = (uint16_t)(g_ng_neogeo_irq_pending &
                                         (uint16_t)~(ack_mask & 0x0007u));
    ng_neogeo_refresh_interrupt_level();
}

int ng_m68k_take_interrupt(uint8_t current_mask, uint8_t *level, uint8_t *vector) {
    uint8_t pending_level = (uint8_t)(g_ng_m68k_interrupt_level & 7u);

    if (!level || !vector || pending_level == 0u) {
        return 0;
    }

    if (pending_level == 7u && g_ng_m68k_level7_edge) {
        g_ng_m68k_level7_edge = 0;
        *level = pending_level;
        *vector = g_ng_m68k_interrupt_vector;
        return 1;
    }

    if (pending_level <= (current_mask & 7u)) {
        return 0;
    }

    *level = pending_level;
    *vector = g_ng_m68k_interrupt_vector;
    return 1;
}
