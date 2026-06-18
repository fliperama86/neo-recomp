#pragma once

#include <stdint.h>

typedef struct NgM68kState {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
    uint32_t usp;
    uint32_t ssp;
} NgM68kState;

