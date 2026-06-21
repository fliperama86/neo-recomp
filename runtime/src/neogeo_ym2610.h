#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NgNeoYm2610 NgNeoYm2610;

NgNeoYm2610 *ng_neogeo_ym2610_create(void);
void ng_neogeo_ym2610_destroy(NgNeoYm2610 *ym);
void ng_neogeo_ym2610_set_roms(NgNeoYm2610 *ym,
                               const uint8_t *v1_rom,
                               uint32_t v1_rom_size,
                               const uint8_t *v2_rom,
                               uint32_t v2_rom_size);
void ng_neogeo_ym2610_reset(NgNeoYm2610 *ym);
uint8_t ng_neogeo_ym2610_read(NgNeoYm2610 *ym, uint8_t port);
void ng_neogeo_ym2610_write(NgNeoYm2610 *ym, uint8_t port, uint8_t data);
void ng_neogeo_ym2610_advance_clocks(NgNeoYm2610 *ym, uint32_t clocks);
void ng_neogeo_ym2610_generate(NgNeoYm2610 *ym,
                               int16_t *stereo_out,
                               uint32_t frames,
                               uint32_t sample_rate);
uint32_t ng_neogeo_ym2610_native_sample_rate(NgNeoYm2610 *ym);
uint8_t ng_neogeo_ym2610_irq_pending(NgNeoYm2610 *ym);

#ifdef __cplusplus
}
#endif
