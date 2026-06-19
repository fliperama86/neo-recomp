#pragma once

#include <stdint.h>

#define NG_GAME_CONFIG_MAX_FUNCTIONS 128u

typedef struct NgGameConfig {
    uint32_t entry[NG_GAME_CONFIG_MAX_FUNCTIONS];
    uint32_t entry_count;
    uint32_t extra[NG_GAME_CONFIG_MAX_FUNCTIONS];
    uint32_t extra_count;
    int truncated;
} NgGameConfig;

void ng_game_config_init(NgGameConfig *config);
int ng_game_config_load(const char *path, NgGameConfig *config);
