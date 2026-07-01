#pragma once

#include <stdint.h>

#define NG_GAME_CONFIG_MAX_FUNCTIONS 2048u
#define NG_GAME_CONFIG_MAX_DISCOVERY_FILES 32u
#define NG_GAME_CONFIG_MAX_JUMP_TABLES 128u
#define NG_GAME_CONFIG_MAX_TABLE_CALLS 64u
#define NG_GAME_CONFIG_MAX_STATE_TABLES 64u
#define NG_GAME_CONFIG_MAX_RECORD_FORMATS 64u
#define NG_GAME_CONFIG_MAX_ROUTINE_TABLES 64u
#define NG_GAME_CONFIG_MAX_RECORD_CALLBACK_OFFSETS 8u
#define NG_GAME_CONFIG_MAX_RECORD_SCANS 16u
#define NG_GAME_CONFIG_MAX_RECORD_NAME 64u
#define NG_GAME_CONFIG_MAX_DISPATCHERS 16u
#define NG_GAME_CONFIG_MAX_DISPATCHER_INSTALL_SLOTS 8u
#define NG_GAME_CONFIG_MAX_DISPATCHER_SPAWN_HELPERS 16u
#define NG_GAME_CONFIG_MAX_RUNTIME_DISPATCHES 64u
#define NG_GAME_CONFIG_MAX_BANKS 256u
#define NG_GAME_CONFIG_MAX_PATH 256u

typedef enum NgGameConfigJumpTableFormat {
    NG_GAME_CONFIG_JUMP_TABLE_ABS32,
    NG_GAME_CONFIG_JUMP_TABLE_PCREL16,
    NG_GAME_CONFIG_JUMP_TABLE_BRA16,
    NG_GAME_CONFIG_JUMP_TABLE_BRA8,
    NG_GAME_CONFIG_JUMP_TABLE_SCRIPT_PREDICATE,
    NG_GAME_CONFIG_JUMP_TABLE_TAGGED_ABS32,
    NG_GAME_CONFIG_JUMP_TABLE_INLINE_CALLBACK,
} NgGameConfigJumpTableFormat;

typedef struct NgGameConfigJumpTable {
    uint32_t start;
    uint32_t end;
    uint32_t stride;
    uint32_t match;
    uint32_t target_offset;
    uint32_t target_start;
    uint32_t target_end;
    NgGameConfigJumpTableFormat format;
} NgGameConfigJumpTable;

typedef enum NgGameConfigTableCallFormat {
    NG_GAME_CONFIG_TABLE_CALL_ABS32_SPARSE,
    NG_GAME_CONFIG_TABLE_CALL_TAGGED_ABS32,
} NgGameConfigTableCallFormat;

typedef struct NgGameConfigTableCall {
    uint32_t helper;
    uint32_t table_start;
    uint32_t table_end;
    uint32_t max_entries;
    uint32_t sentinel;
    uint32_t stride;
    uint32_t match;
    uint32_t target_offset;
    uint32_t target_start;
    uint32_t target_end;
    uint8_t table_reg;
    NgGameConfigTableCallFormat format;
} NgGameConfigTableCall;

typedef enum NgGameConfigRecordScanKind {
    NG_GAME_CONFIG_RECORD_SCAN_FIXED,
    NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL,
    NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE,
    NG_GAME_CONFIG_RECORD_SCAN_RANGE,
} NgGameConfigRecordScanKind;

typedef struct NgGameConfigRecordScan {
    NgGameConfigRecordScanKind kind;
    uint32_t bank_id;
    uint32_t start;
    uint32_t end;
} NgGameConfigRecordScan;

typedef struct NgGameConfigStateTable {
    uint32_t root;
    uint32_t table_start;
    uint32_t table_end;
    uint32_t stride;
    uint32_t sentinel;
    uint32_t target_start;
    uint32_t target_end;
    uint32_t max_tables;
    uint32_t max_entries;
    int follow_chain;
    int skip_invalid_targets;
    NgGameConfigRecordScan scans[NG_GAME_CONFIG_MAX_RECORD_SCANS];
    uint32_t scan_count;
} NgGameConfigStateTable;

typedef struct NgGameConfigRecordFormat {
    char name[NG_GAME_CONFIG_MAX_RECORD_NAME];
    uint32_t tag;
    uint32_t tag_offset;
    int has_tag;
    uint32_t width;
    uint32_t stride;
    uint32_t callback_offsets[NG_GAME_CONFIG_MAX_RECORD_CALLBACK_OFFSETS];
    uint32_t callback_offset_count;
    uint32_t sentinel;
    int has_sentinel;
    uint32_t cluster_min_entries;
    uint32_t cluster_max_entries;
    uint32_t target_start;
    uint32_t target_end;
    NgGameConfigRecordScan scans[NG_GAME_CONFIG_MAX_RECORD_SCANS];
    uint32_t scan_count;
} NgGameConfigRecordFormat;

typedef struct NgGameConfigRoutineTable {
    char name[NG_GAME_CONFIG_MAX_RECORD_NAME];
    uint32_t stride;
    uint32_t width;
    uint32_t min_instructions;
    uint32_t fallthrough_target;
    int has_fallthrough_target;
    NgGameConfigRecordScan scans[NG_GAME_CONFIG_MAX_RECORD_SCANS];
    uint32_t scan_count;
} NgGameConfigRoutineTable;

typedef enum NgGameConfigDispatcherKind {
    NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE,
} NgGameConfigDispatcherKind;

typedef struct NgGameConfigDispatcher {
    NgGameConfigDispatcherKind kind;
    uint32_t state_slot;
    int has_state_slot;
    uint32_t install_slots[NG_GAME_CONFIG_MAX_DISPATCHER_INSTALL_SLOTS];
    uint32_t install_slot_count;
    uint32_t spawn_helpers[NG_GAME_CONFIG_MAX_DISPATCHER_SPAWN_HELPERS];
    uint32_t spawn_helper_count;
} NgGameConfigDispatcher;

typedef struct NgGameConfigBank {
    uint32_t id;
    uint32_t offset;
    uint32_t size;
    int has_offset;
    int has_size;
} NgGameConfigBank;

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
    NgGameConfigTableCall table_calls[NG_GAME_CONFIG_MAX_TABLE_CALLS];
    uint32_t table_call_count;
    NgGameConfigStateTable state_tables[NG_GAME_CONFIG_MAX_STATE_TABLES];
    uint32_t state_table_count;
    NgGameConfigRecordFormat record_formats[NG_GAME_CONFIG_MAX_RECORD_FORMATS];
    uint32_t record_format_count;
    NgGameConfigRoutineTable routine_tables[NG_GAME_CONFIG_MAX_ROUTINE_TABLES];
    uint32_t routine_table_count;
    NgGameConfigDispatcher dispatchers[NG_GAME_CONFIG_MAX_DISPATCHERS];
    uint32_t dispatcher_count;
    NgGameConfigBank banks[NG_GAME_CONFIG_MAX_BANKS];
    uint32_t bank_count;
    uint32_t runtime_dispatch[NG_GAME_CONFIG_MAX_RUNTIME_DISPATCHES];
    uint32_t runtime_dispatch_count;
    int truncated;
} NgGameConfig;

void ng_game_config_init(NgGameConfig *config);
int ng_game_config_load(const char *path, NgGameConfig *config);
