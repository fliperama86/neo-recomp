#pragma once

#include <stdint.h>

#define NG_GAME_CONFIG_MAX_FUNCTIONS 2048u
#define NG_GAME_CONFIG_MAX_DISCOVERY_FILES 32u
#define NG_GAME_CONFIG_MAX_JUMP_TABLES 128u
#define NG_GAME_CONFIG_MAX_RUNTIME_DISPATCHES 64u
#define NG_GAME_CONFIG_MAX_PATH 256u

typedef enum NgGameConfigJumpTableFormat {
    NG_GAME_CONFIG_JUMP_TABLE_ABS32,
    NG_GAME_CONFIG_JUMP_TABLE_PCREL16,
    NG_GAME_CONFIG_JUMP_TABLE_BRA16,
    NG_GAME_CONFIG_JUMP_TABLE_BRA8,
} NgGameConfigJumpTableFormat;

typedef struct NgGameConfigJumpTable {
    uint32_t start;
    uint32_t end;
    uint32_t stride;
    NgGameConfigJumpTableFormat format;
} NgGameConfigJumpTable;

typedef struct NgGameConfig {
    uint32_t program_fixed_base;
    uint32_t program_fixed_size;
    uint32_t program_bank_window_base;
    uint32_t program_bank_window_size;
    int program_map_configured;
    uint32_t entry[NG_GAME_CONFIG_MAX_FUNCTIONS];
    uint32_t entry_count;
    uint32_t extra[NG_GAME_CONFIG_MAX_FUNCTIONS];
    uint32_t extra_count;
    char discovery_files[NG_GAME_CONFIG_MAX_DISCOVERY_FILES][NG_GAME_CONFIG_MAX_PATH];
    uint32_t discovery_file_count;
    NgGameConfigJumpTable jump_tables[NG_GAME_CONFIG_MAX_JUMP_TABLES];
    uint32_t jump_table_count;
    uint32_t runtime_dispatch[NG_GAME_CONFIG_MAX_RUNTIME_DISPATCHES];
    uint32_t runtime_dispatch_count;
    int truncated;
} NgGameConfig;

void ng_game_config_init(NgGameConfig *config);
int ng_game_config_load(const char *path, NgGameConfig *config);
