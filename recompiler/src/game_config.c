#include "game_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum NgGameConfigSection {
    NG_GAME_CONFIG_SECTION_NONE,
    NG_GAME_CONFIG_SECTION_GAME,
    NG_GAME_CONFIG_SECTION_PROGRAM,
    NG_GAME_CONFIG_SECTION_FUNCTIONS,
    NG_GAME_CONFIG_SECTION_DISPATCH,
    NG_GAME_CONFIG_SECTION_JUMP_TABLE,
    NG_GAME_CONFIG_SECTION_TABLE_CALL,
    NG_GAME_CONFIG_SECTION_STATE_TABLE,
    NG_GAME_CONFIG_SECTION_RECORD_FORMAT,
    NG_GAME_CONFIG_SECTION_ROUTINE_TABLE,
    NG_GAME_CONFIG_SECTION_DISPATCHER,
    NG_GAME_CONFIG_SECTION_BANK,
} NgGameConfigSection;

typedef enum NgGameConfigArray {
    NG_GAME_CONFIG_ARRAY_NONE,
    NG_GAME_CONFIG_ARRAY_ENTRY,
    NG_GAME_CONFIG_ARRAY_EXTRA,
    NG_GAME_CONFIG_ARRAY_DISCOVERY_FILES,
    NG_GAME_CONFIG_ARRAY_RUNTIME_DISPATCH,
    NG_GAME_CONFIG_ARRAY_RECORD_CALLBACK_OFFSETS,
    NG_GAME_CONFIG_ARRAY_RECORD_SCAN,
    NG_GAME_CONFIG_ARRAY_STATE_TABLE_SCAN,
    NG_GAME_CONFIG_ARRAY_ROUTINE_TABLE_SCAN,
    NG_GAME_CONFIG_ARRAY_DISPATCHER_INSTALL_SLOTS,
    NG_GAME_CONFIG_ARRAY_DISPATCHER_SPAWN_HELPERS,
} NgGameConfigArray;

typedef struct NgGameConfigPathList {
    char paths[NG_GAME_CONFIG_MAX_DISCOVERY_FILES][NG_GAME_CONFIG_MAX_PATH];
    uint32_t count;
} NgGameConfigPathList;

void ng_game_config_init(NgGameConfig *config) {
    if (config) {
        memset(config, 0, sizeof(*config));
    }
}

static char *trim_left(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    return s;
}

static int key_starts_array(const char *line, const char *key) {
    size_t len = strlen(key);
    if (strncmp(line, key, len) != 0) {
        return 0;
    }
    line += len;
    while (*line && isspace((unsigned char)*line)) {
        ++line;
    }
    return *line == '=';
}

static const char *key_value_start(const char *line, const char *key) {
    size_t len = strlen(key);
    if (strncmp(line, key, len) != 0) {
        return NULL;
    }
    line += len;
    while (*line && isspace((unsigned char)*line)) {
        ++line;
    }
    if (*line != '=') {
        return NULL;
    }
    ++line;
    while (*line && isspace((unsigned char)*line)) {
        ++line;
    }
    return line;
}

static int is_path_separator(char c) {
    return c == '/' || c == '\\';
}

static const char *last_path_separator(const char *path) {
    const char *last = NULL;
    if (!path) {
        return NULL;
    }
    for (const char *p = path; *p; ++p) {
        if (is_path_separator(*p)) {
            last = p;
        }
    }
    return last;
}

static int is_absolute_path(const char *path) {
    if (!path || !*path) {
        return 0;
    }
    if (is_path_separator(path[0])) {
        return 1;
    }
    return isalpha((unsigned char)path[0]) && path[1] == ':';
}

static char join_separator_for_base(const char *base) {
    return base && strchr(base, '\\') ? '\\' : '/';
}

static void dirname_of(const char *path, char *out, size_t out_size) {
    if (!out || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (!path || !*path) {
        snprintf(out, out_size, ".");
        return;
    }

    const char *slash = last_path_separator(path);
    if (!slash) {
        snprintf(out, out_size, ".");
        return;
    }
    size_t len = (size_t)(slash - path);
    if (len == 0u) {
        len = 1u;
    } else if (len == 2u && path[1] == ':' && is_path_separator(path[2])) {
        len = 3u;
    }
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, path, len);
    out[len] = '\0';
}

static void join_path(const char *base,
                      const char *path,
                      char *out,
                      size_t out_size) {
    if (!out || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (!path || !*path) {
        return;
    }
    if (is_absolute_path(path)) {
        snprintf(out, out_size, "%s", path);
    } else if (!base || !*base || strcmp(base, ".") == 0) {
        snprintf(out, out_size, "%s", path);
    } else if (is_path_separator(base[strlen(base) - 1u])) {
        snprintf(out, out_size, "%s%s", base, path);
    } else {
        snprintf(out, out_size, "%s%c%s", base, join_separator_for_base(base), path);
    }
}

static void append_function_addr(NgGameConfig *config,
                                 NgGameConfigArray array,
                                 uint32_t value) {
    uint32_t *count = NULL;
    uint32_t *items = NULL;

    if (array == NG_GAME_CONFIG_ARRAY_ENTRY) {
        count = &config->entry_count;
        items = config->entry;
    } else if (array == NG_GAME_CONFIG_ARRAY_EXTRA) {
        count = &config->extra_count;
        items = config->extra;
    } else if (array == NG_GAME_CONFIG_ARRAY_RUNTIME_DISPATCH) {
        count = &config->runtime_dispatch_count;
        items = config->runtime_dispatch;
    } else {
        return;
    }

    uint32_t max_count =
        array == NG_GAME_CONFIG_ARRAY_RUNTIME_DISPATCH ?
        NG_GAME_CONFIG_MAX_RUNTIME_DISPATCHES :
        NG_GAME_CONFIG_MAX_FUNCTIONS;
    if (*count >= max_count) {
        config->truncated = 1;
        return;
    }
    items[(*count)++] = value;
}

static NgGameConfigJumpTable *append_jump_table(NgGameConfig *config) {
    if (!config) {
        return NULL;
    }
    if (config->jump_table_count >= NG_GAME_CONFIG_MAX_JUMP_TABLES) {
        config->truncated = 1;
        return NULL;
    }

    NgGameConfigJumpTable *table =
        &config->jump_tables[config->jump_table_count++];
    memset(table, 0, sizeof(*table));
    table->stride = 4u;
    table->format = NG_GAME_CONFIG_JUMP_TABLE_ABS32;
    return table;
}

static NgGameConfigTableCall *append_table_call(NgGameConfig *config) {
    if (!config) {
        return NULL;
    }
    if (config->table_call_count >= NG_GAME_CONFIG_MAX_TABLE_CALLS) {
        config->truncated = 1;
        return NULL;
    }

    NgGameConfigTableCall *call =
        &config->table_calls[config->table_call_count++];
    memset(call, 0, sizeof(*call));
    call->max_entries = 16u;
    call->sentinel = 0xFFFFFFFFu;
    call->stride = 4u;
    call->format = NG_GAME_CONFIG_TABLE_CALL_ABS32_SPARSE;
    return call;
}

static NgGameConfigStateTable *append_state_table(NgGameConfig *config) {
    if (!config) {
        return NULL;
    }
    if (config->state_table_count >= NG_GAME_CONFIG_MAX_STATE_TABLES) {
        config->truncated = 1;
        return NULL;
    }

    NgGameConfigStateTable *table =
        &config->state_tables[config->state_table_count++];
    memset(table, 0, sizeof(*table));
    table->stride = 4u;
    table->sentinel = 0xFFFFFFFFu;
    table->max_tables = 64u;
    table->max_entries = 1024u;
    return table;
}

static NgGameConfigRecordFormat *append_record_format(NgGameConfig *config) {
    if (!config) {
        return NULL;
    }
    if (config->record_format_count >= NG_GAME_CONFIG_MAX_RECORD_FORMATS) {
        config->truncated = 1;
        return NULL;
    }

    NgGameConfigRecordFormat *record =
        &config->record_formats[config->record_format_count++];
    memset(record, 0, sizeof(*record));
    record->sentinel = 0xFFFFFFFFu;
    return record;
}

static NgGameConfigRoutineTable *append_routine_table(NgGameConfig *config) {
    if (!config) {
        return NULL;
    }
    if (config->routine_table_count >= NG_GAME_CONFIG_MAX_ROUTINE_TABLES) {
        config->truncated = 1;
        return NULL;
    }

    NgGameConfigRoutineTable *table =
        &config->routine_tables[config->routine_table_count++];
    memset(table, 0, sizeof(*table));
    table->min_instructions = 1u;
    return table;
}

static NgGameConfigDispatcher *append_dispatcher(NgGameConfig *config) {
    if (!config) {
        return NULL;
    }
    if (config->dispatcher_count >= NG_GAME_CONFIG_MAX_DISPATCHERS) {
        config->truncated = 1;
        return NULL;
    }

    NgGameConfigDispatcher *dispatcher =
        &config->dispatchers[config->dispatcher_count++];
    memset(dispatcher, 0, sizeof(*dispatcher));
    dispatcher->kind = NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE;
    return dispatcher;
}

static NgGameConfigBank *append_bank(NgGameConfig *config) {
    if (!config) {
        return NULL;
    }
    if (config->bank_count >= NG_GAME_CONFIG_MAX_BANKS) {
        config->truncated = 1;
        return NULL;
    }

    NgGameConfigBank *bank = &config->banks[config->bank_count];
    memset(bank, 0, sizeof(*bank));
    bank->id = config->bank_count;
    ++config->bank_count;
    return bank;
}

static void append_discovery_file(NgGameConfig *config,
                                  NgGameConfigPathList *local,
                                  const char *value) {
    if (!config || !local || !value || !*value) {
        return;
    }

    if (local->count >= NG_GAME_CONFIG_MAX_DISCOVERY_FILES ||
        config->discovery_file_count >= NG_GAME_CONFIG_MAX_DISCOVERY_FILES) {
        config->truncated = 1;
        return;
    }

    snprintf(local->paths[local->count],
             sizeof(local->paths[local->count]),
             "%s",
             value);
    ++local->count;

    snprintf(config->discovery_files[config->discovery_file_count],
             sizeof(config->discovery_files[config->discovery_file_count]),
             "%s",
             value);
    ++config->discovery_file_count;
}

static void parse_address_values(char *line,
                                 NgGameConfigArray array,
                                 NgGameConfig *config) {
    char *p = line;

    while (*p) {
        if (*p == '#') {
            break;
        }
        if (isdigit((unsigned char)*p)) {
            char *end = p;
            unsigned long value = strtoul(p, &end, 0);
            if (end != p) {
                append_function_addr(config, array, (uint32_t)value);
                p = end;
                continue;
            }
        }
        ++p;
    }
}

static void parse_string_values(char *line,
                                NgGameConfig *config,
                                NgGameConfigPathList *local) {
    char *p = line;
    while (*p) {
        if (*p == '#') {
            break;
        }
        if (*p != '"') {
            ++p;
            continue;
        }

        char *start = ++p;
        while (*p && *p != '"') {
            ++p;
        }
        if (*p != '"') {
            break;
        }

        char saved = *p;
        *p = '\0';
        append_discovery_file(config, local, start);
        *p = saved;
        ++p;
    }
}

static void append_config_values(NgGameConfig *dst, const NgGameConfig *src) {
    if (!dst || !src) {
        return;
    }
    if (src->program_map_configured) {
        dst->program_fixed_base = src->program_fixed_base;
        dst->program_fixed_size = src->program_fixed_size;
        dst->program_bank_window_base = src->program_bank_window_base;
        dst->program_bank_window_size = src->program_bank_window_size;
        dst->program_map_configured = 1;
    }
    for (uint32_t i = 0; i < src->entry_count; ++i) {
        append_function_addr(dst, NG_GAME_CONFIG_ARRAY_ENTRY, src->entry[i]);
    }
    for (uint32_t i = 0; i < src->extra_count; ++i) {
        append_function_addr(dst, NG_GAME_CONFIG_ARRAY_EXTRA, src->extra[i]);
    }
    for (uint32_t i = 0; i < src->runtime_dispatch_count; ++i) {
        append_function_addr(dst,
                             NG_GAME_CONFIG_ARRAY_RUNTIME_DISPATCH,
                             src->runtime_dispatch[i]);
    }
    for (uint32_t i = 0; i < src->jump_table_count; ++i) {
        NgGameConfigJumpTable *table = append_jump_table(dst);
        if (table) {
            *table = src->jump_tables[i];
        }
    }
    for (uint32_t i = 0; i < src->table_call_count; ++i) {
        NgGameConfigTableCall *call = append_table_call(dst);
        if (call) {
            *call = src->table_calls[i];
        }
    }
    for (uint32_t i = 0; i < src->state_table_count; ++i) {
        NgGameConfigStateTable *table = append_state_table(dst);
        if (table) {
            *table = src->state_tables[i];
        }
    }
    for (uint32_t i = 0; i < src->record_format_count; ++i) {
        NgGameConfigRecordFormat *record = append_record_format(dst);
        if (record) {
            *record = src->record_formats[i];
        }
    }
    for (uint32_t i = 0; i < src->routine_table_count; ++i) {
        NgGameConfigRoutineTable *table = append_routine_table(dst);
        if (table) {
            *table = src->routine_tables[i];
        }
    }
    for (uint32_t i = 0; i < src->dispatcher_count; ++i) {
        NgGameConfigDispatcher *dispatcher = append_dispatcher(dst);
        if (dispatcher) {
            *dispatcher = src->dispatchers[i];
        }
    }
    for (uint32_t i = 0; i < src->bank_count; ++i) {
        NgGameConfigBank *bank = append_bank(dst);
        if (bank) {
            *bank = src->banks[i];
        }
    }
    if (src->truncated) {
        dst->truncated = 1;
    }
}

static void parse_jump_table_format(const char *value,
                                    NgGameConfigJumpTable *table) {
    if (!value || !table) {
        return;
    }
    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    if (*value == '"') {
        ++value;
    }

    char token[32];
    size_t len = 0;
    while (value[len] &&
           value[len] != '"' &&
           value[len] != ',' &&
           !isspace((unsigned char)value[len]) &&
           len + 1u < sizeof(token)) {
        token[len] = value[len];
        ++len;
    }
    token[len] = '\0';

    if (strcmp(token, "pcrel16") == 0 ||
        strcmp(token, "pcrel_w") == 0) {
        table->format = NG_GAME_CONFIG_JUMP_TABLE_PCREL16;
    } else if (strcmp(token, "bra16") == 0 ||
               strcmp(token, "bra_w") == 0) {
        table->format = NG_GAME_CONFIG_JUMP_TABLE_BRA16;
    } else if (strcmp(token, "bra8") == 0 ||
               strcmp(token, "bra_s") == 0) {
        table->format = NG_GAME_CONFIG_JUMP_TABLE_BRA8;
    } else if (strcmp(token, "script_predicate") == 0 ||
               strcmp(token, "script_predicates") == 0) {
        table->format = NG_GAME_CONFIG_JUMP_TABLE_SCRIPT_PREDICATE;
    } else if (strcmp(token, "tagged_abs32") == 0 ||
               strcmp(token, "tagged_long") == 0) {
        table->format = NG_GAME_CONFIG_JUMP_TABLE_TAGGED_ABS32;
    } else if (strcmp(token, "inline_callback") == 0 ||
               strcmp(token, "script_callback") == 0) {
        table->format = NG_GAME_CONFIG_JUMP_TABLE_INLINE_CALLBACK;
    } else {
        table->format = NG_GAME_CONFIG_JUMP_TABLE_ABS32;
    }
}

static void parse_jump_table_scalar(char *line,
                                    NgGameConfigJumpTable *table) {
    if (!line || !table) {
        return;
    }

    const char *value = key_value_start(line, "start");
    if (value) {
        table->start = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "end");
    if (value) {
        table->end = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "stride");
    if (value) {
        table->stride = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "match");
    if (value) {
        table->match = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_offset");
    if (value) {
        table->target_offset = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_start");
    if (value) {
        table->target_start = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_end");
    if (value) {
        table->target_end = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "format");
    if (value) {
        parse_jump_table_format(value, table);
    }
}

static uint8_t parse_areg_value(const char *value) {
    if (!value) {
        return 0u;
    }
    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    if (*value == '"') {
        ++value;
    }
    if ((value[0] == 'A' || value[0] == 'a') &&
        value[1] >= '0' && value[1] <= '7') {
        return (uint8_t)(value[1] - '0');
    }
    unsigned long reg = strtoul(value, NULL, 0);
    return reg <= 7ul ? (uint8_t)reg : 0u;
}

static void parse_table_call_format(const char *value,
                                    NgGameConfigTableCall *call) {
    if (!value || !call) {
        return;
    }
    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    if (*value == '"') {
        ++value;
    }

    char token[32];
    size_t len = 0;
    while (value[len] &&
           value[len] != '"' &&
           value[len] != ',' &&
           !isspace((unsigned char)value[len]) &&
           len + 1u < sizeof(token)) {
        token[len] = value[len];
        ++len;
    }
    token[len] = '\0';

    if (strcmp(token, "abs32_sparse") == 0 ||
        strcmp(token, "abs32") == 0) {
        call->format = NG_GAME_CONFIG_TABLE_CALL_ABS32_SPARSE;
    } else if (strcmp(token, "tagged_abs32") == 0 ||
               strcmp(token, "tagged_long") == 0) {
        call->format = NG_GAME_CONFIG_TABLE_CALL_TAGGED_ABS32;
    }
}

static void parse_table_call_scalar(char *line,
                                    NgGameConfigTableCall *call) {
    if (!line || !call) {
        return;
    }

    const char *value = key_value_start(line, "helper");
    if (value) {
        call->helper = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "table_start");
    if (value) {
        call->table_start = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "table_end");
    if (value) {
        call->table_end = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "max_entries");
    if (value) {
        call->max_entries = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "sentinel");
    if (value) {
        call->sentinel = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "stride");
    if (value) {
        call->stride = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "match");
    if (value) {
        call->match = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_offset");
    if (value) {
        call->target_offset = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_start");
    if (value) {
        call->target_start = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_end");
    if (value) {
        call->target_end = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "table_reg");
    if (value) {
        call->table_reg = parse_areg_value(value);
        return;
    }
    value = key_value_start(line, "format");
    if (value) {
        parse_table_call_format(value, call);
    }
}

static int parse_bool_value(const char *value) {
    if (!value) {
        return 0;
    }
    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    if (*value == '"') {
        ++value;
    }
    return *value == '1' ||
           strncmp(value, "true", sizeof("true") - 1u) == 0 ||
           strncmp(value, "yes", sizeof("yes") - 1u) == 0 ||
           strncmp(value, "on", sizeof("on") - 1u) == 0;
}

static int parse_range_value(const char *value,
                             uint32_t *start,
                             uint32_t *end) {
    if (!value || !start || !end) {
        return 0;
    }
    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    if (*value == '"') {
        ++value;
    }

    char *mid = NULL;
    unsigned long lo = strtoul(value, &mid, 0);
    if (!mid || mid == value) {
        return 0;
    }
    while (*mid && isspace((unsigned char)*mid)) {
        ++mid;
    }
    if (*mid != '-') {
        return 0;
    }
    ++mid;
    while (*mid && isspace((unsigned char)*mid)) {
        ++mid;
    }

    char *tail = NULL;
    unsigned long hi = strtoul(mid, &tail, 0);
    if (!tail || tail == mid) {
        return 0;
    }

    *start = (uint32_t)lo;
    *end = (uint32_t)hi;
    return 1;
}

static void parse_state_table_scalar(char *line,
                                     NgGameConfigStateTable *table) {
    if (!line || !table) {
        return;
    }

    const char *value = key_value_start(line, "root");
    if (value) {
        table->root = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "table_start");
    if (value) {
        table->table_start = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "table_end");
    if (value) {
        table->table_end = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "stride");
    if (value) {
        table->stride = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "sentinel");
    if (value) {
        table->sentinel = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "follow_chain");
    if (value) {
        table->follow_chain = parse_bool_value(value);
        return;
    }
    value = key_value_start(line, "skip_invalid_targets");
    if (value) {
        table->skip_invalid_targets = parse_bool_value(value);
        return;
    }
    value = key_value_start(line, "target");
    if (value) {
        parse_range_value(value, &table->target_start, &table->target_end);
        return;
    }
    value = key_value_start(line, "target_start");
    if (value) {
        table->target_start = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_end");
    if (value) {
        table->target_end = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "max_tables");
    if (value) {
        table->max_tables = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "max_entries");
    if (value) {
        table->max_entries = (uint32_t)strtoul(value, NULL, 0);
    }
}

static void parse_string_scalar(const char *value,
                                char *out,
                                size_t out_size) {
    if (!value || !out || out_size == 0u) {
        return;
    }

    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    if (*value == '"') {
        ++value;
    }

    size_t len = 0;
    while (value[len] &&
           value[len] != '"' &&
           value[len] != '\n' &&
           value[len] != '\r' &&
           len + 1u < out_size) {
        out[len] = value[len];
        ++len;
    }
    out[len] = '\0';
}

static void append_record_callback_offset(NgGameConfig *config,
                                          NgGameConfigRecordFormat *record,
                                          uint32_t value) {
    if (!config || !record) {
        return;
    }
    if (record->callback_offset_count >=
        NG_GAME_CONFIG_MAX_RECORD_CALLBACK_OFFSETS) {
        config->truncated = 1;
        return;
    }
    record->callback_offsets[record->callback_offset_count++] = value;
}

static void parse_record_callback_offsets(char *line,
                                          NgGameConfig *config,
                                          NgGameConfigRecordFormat *record) {
    if (!line || !record) {
        return;
    }

    char *p = strchr(line, '=');
    p = p ? p + 1 : line;
    while (*p) {
        if (*p == '#') {
            break;
        }
        if (isdigit((unsigned char)*p)) {
            char *end = p;
            unsigned long value = strtoul(p, &end, 0);
            if (end != p) {
                append_record_callback_offset(config, record, (uint32_t)value);
                p = end;
                continue;
            }
        }
        ++p;
    }
}

static void append_record_scan_token(NgGameConfig *config,
                                     NgGameConfigRecordFormat *record,
                                     const char *token) {
    if (!config || !record || !token || !*token) {
        return;
    }
    if (record->scan_count >= NG_GAME_CONFIG_MAX_RECORD_SCANS) {
        config->truncated = 1;
        return;
    }

    NgGameConfigRecordScan *scan = &record->scans[record->scan_count];
    memset(scan, 0, sizeof(*scan));
    if (strcmp(token, "fixed") == 0) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_FIXED;
    } else if (strcmp(token, "bank:*") == 0) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL;
    } else if (strncmp(token, "bank:", 5u) == 0 && token[5] != '\0') {
        char *end = NULL;
        unsigned long bank = strtoul(token + 5u, &end, 0);
        if (end == token + 5u || *end != '\0') {
            return;
        }
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE;
        scan->bank_id = (uint32_t)bank;
    } else if (parse_range_value(token, &scan->start, &scan->end)) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_RANGE;
    } else {
        return;
    }
    ++record->scan_count;
}

static void append_state_table_scan_token(NgGameConfig *config,
                                          NgGameConfigStateTable *table,
                                          const char *token) {
    if (!config || !table || !token || !*token) {
        return;
    }
    if (table->scan_count >= NG_GAME_CONFIG_MAX_RECORD_SCANS) {
        config->truncated = 1;
        return;
    }

    NgGameConfigRecordScan *scan = &table->scans[table->scan_count];
    memset(scan, 0, sizeof(*scan));
    if (strcmp(token, "fixed") == 0) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_FIXED;
    } else if (strcmp(token, "bank:*") == 0) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL;
    } else if (strncmp(token, "bank:", 5u) == 0 && token[5] != '\0') {
        char *end = NULL;
        unsigned long bank = strtoul(token + 5u, &end, 0);
        if (end == token + 5u || *end != '\0') {
            return;
        }
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE;
        scan->bank_id = (uint32_t)bank;
    } else if (parse_range_value(token, &scan->start, &scan->end)) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_RANGE;
    } else {
        return;
    }
    ++table->scan_count;
}

static void append_routine_table_scan_token(NgGameConfig *config,
                                            NgGameConfigRoutineTable *table,
                                            const char *token) {
    if (!config || !table || !token || !*token) {
        return;
    }
    if (table->scan_count >= NG_GAME_CONFIG_MAX_RECORD_SCANS) {
        config->truncated = 1;
        return;
    }

    NgGameConfigRecordScan *scan = &table->scans[table->scan_count];
    memset(scan, 0, sizeof(*scan));
    if (strcmp(token, "fixed") == 0) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_FIXED;
    } else if (strcmp(token, "bank:*") == 0) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL;
    } else if (strncmp(token, "bank:", 5u) == 0 && token[5] != '\0') {
        char *end = NULL;
        unsigned long bank = strtoul(token + 5u, &end, 0);
        if (end == token + 5u || *end != '\0') {
            return;
        }
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE;
        scan->bank_id = (uint32_t)bank;
    } else if (parse_range_value(token, &scan->start, &scan->end)) {
        scan->kind = NG_GAME_CONFIG_RECORD_SCAN_RANGE;
    } else {
        return;
    }
    ++table->scan_count;
}

static void parse_record_scan_values(char *line,
                                     NgGameConfig *config,
                                     NgGameConfigRecordFormat *record) {
    if (!line || !record) {
        return;
    }

    char *p = strchr(line, '=');
    p = p ? p + 1 : line;
    while (*p) {
        if (*p == '#') {
            break;
        }
        if (*p != '"') {
            ++p;
            continue;
        }

        char *start = ++p;
        while (*p && *p != '"') {
            ++p;
        }
        if (*p != '"') {
            break;
        }
        char saved = *p;
        *p = '\0';
        append_record_scan_token(config, record, start);
        *p = saved;
        ++p;
    }
}

static void parse_state_table_scan_values(char *line,
                                          NgGameConfig *config,
                                          NgGameConfigStateTable *table) {
    if (!line || !table) {
        return;
    }

    char *p = strchr(line, '=');
    p = p ? p + 1 : line;
    while (*p) {
        if (*p == '#') {
            break;
        }
        if (*p != '"') {
            ++p;
            continue;
        }
        char *start = ++p;
        while (*p && *p != '"') {
            ++p;
        }
        if (*p != '"') {
            break;
        }
        char saved = *p;
        *p = '\0';
        append_state_table_scan_token(config, table, start);
        *p = saved;
        ++p;
    }
}

static void parse_routine_table_scan_values(
    char *line,
    NgGameConfig *config,
    NgGameConfigRoutineTable *table) {
    if (!line || !table) {
        return;
    }

    char *p = strchr(line, '=');
    p = p ? p + 1 : line;
    while (*p) {
        if (*p == '#') {
            break;
        }
        if (*p != '"') {
            ++p;
            continue;
        }
        char *start = ++p;
        while (*p && *p != '"') {
            ++p;
        }
        if (*p != '"') {
            break;
        }
        char saved = *p;
        *p = '\0';
        append_routine_table_scan_token(config, table, start);
        *p = saved;
        ++p;
    }
}

static void parse_record_format_scalar(char *line,
                                       NgGameConfigRecordFormat *record) {
    if (!line || !record) {
        return;
    }

    const char *value = key_value_start(line, "name");
    if (value) {
        parse_string_scalar(value, record->name, sizeof(record->name));
        return;
    }
    value = key_value_start(line, "tag_offset");
    if (value) {
        record->tag_offset = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "tag");
    if (value) {
        record->tag = (uint32_t)strtoul(value, NULL, 0);
        record->has_tag = 1;
        return;
    }
    value = key_value_start(line, "width");
    if (value) {
        record->width = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "stride");
    if (value) {
        record->stride = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "sentinel");
    if (value) {
        record->sentinel = (uint32_t)strtoul(value, NULL, 0);
        record->has_sentinel = 1;
        return;
    }
    value = key_value_start(line, "target");
    if (value) {
        parse_range_value(value, &record->target_start, &record->target_end);
        return;
    }
    value = key_value_start(line, "target_start");
    if (value) {
        record->target_start = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "target_end");
    if (value) {
        record->target_end = (uint32_t)strtoul(value, NULL, 0);
    }
}

static void parse_routine_table_scalar(char *line,
                                       NgGameConfigRoutineTable *table) {
    if (!line || !table) {
        return;
    }

    const char *value = key_value_start(line, "name");
    if (value) {
        parse_string_scalar(value, table->name, sizeof(table->name));
        return;
    }
    value = key_value_start(line, "stride");
    if (value) {
        table->stride = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "width");
    if (value) {
        table->width = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "min_instructions");
    if (value) {
        table->min_instructions = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "fallthrough_target");
    if (!value) {
        value = key_value_start(line, "shared_tail");
    }
    if (value) {
        table->fallthrough_target = (uint32_t)strtoul(value, NULL, 0);
        table->has_fallthrough_target = 1;
    }
}

static void parse_dispatcher_kind(const char *value,
                                  NgGameConfigDispatcher *dispatcher) {
    if (!value || !dispatcher) {
        return;
    }

    char token[32];
    parse_string_scalar(value, token, sizeof(token));
    if (strcmp(token, "object_state") == 0) {
        dispatcher->kind = NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE;
    }
}

static void append_dispatcher_array_value(NgGameConfig *config,
                                          NgGameConfigDispatcher *dispatcher,
                                          NgGameConfigArray array,
                                          uint32_t value) {
    if (!config || !dispatcher) {
        return;
    }

    if (array == NG_GAME_CONFIG_ARRAY_DISPATCHER_INSTALL_SLOTS) {
        if (dispatcher->install_slot_count >=
            NG_GAME_CONFIG_MAX_DISPATCHER_INSTALL_SLOTS) {
            config->truncated = 1;
            return;
        }
        dispatcher->install_slots[dispatcher->install_slot_count++] = value;
    } else if (array == NG_GAME_CONFIG_ARRAY_DISPATCHER_SPAWN_HELPERS) {
        if (dispatcher->spawn_helper_count >=
            NG_GAME_CONFIG_MAX_DISPATCHER_SPAWN_HELPERS) {
            config->truncated = 1;
            return;
        }
        dispatcher->spawn_helpers[dispatcher->spawn_helper_count++] = value;
    }
}

static void parse_dispatcher_array_values(char *line,
                                          NgGameConfig *config,
                                          NgGameConfigDispatcher *dispatcher,
                                          NgGameConfigArray array) {
    if (!line || !dispatcher) {
        return;
    }

    char *p = strchr(line, '=');
    p = p ? p + 1 : line;
    while (*p) {
        if (*p == '#') {
            break;
        }
        if (isdigit((unsigned char)*p)) {
            char *end = p;
            unsigned long value = strtoul(p, &end, 0);
            if (end != p) {
                append_dispatcher_array_value(config,
                                              dispatcher,
                                              array,
                                              (uint32_t)value);
                p = end;
                continue;
            }
        }
        ++p;
    }
}

static void parse_dispatcher_scalar(char *line,
                                    NgGameConfigDispatcher *dispatcher) {
    if (!line || !dispatcher) {
        return;
    }

    const char *value = key_value_start(line, "kind");
    if (value) {
        parse_dispatcher_kind(value, dispatcher);
        return;
    }
    value = key_value_start(line, "state_slot");
    if (value) {
        dispatcher->state_slot = (uint32_t)strtoul(value, NULL, 0);
        dispatcher->has_state_slot = 1;
    }
}

static void parse_bank_scalar(char *line, NgGameConfigBank *bank) {
    if (!line || !bank) {
        return;
    }

    const char *value = key_value_start(line, "id");
    if (value) {
        bank->id = (uint32_t)strtoul(value, NULL, 0);
        return;
    }
    value = key_value_start(line, "offset");
    if (!value) {
        value = key_value_start(line, "physical_offset");
    }
    if (value) {
        bank->offset = (uint32_t)strtoul(value, NULL, 0);
        bank->has_offset = 1;
        return;
    }
    value = key_value_start(line, "size");
    if (value) {
        bank->size = (uint32_t)strtoul(value, NULL, 0);
        bank->has_size = 1;
    }
}

static void parse_program_scalar(char *line, NgGameConfig *config) {
    if (!line || !config) {
        return;
    }

    const char *value = key_value_start(line, "fixed_base");
    if (value) {
        config->program_fixed_base = (uint32_t)strtoul(value, NULL, 0);
        config->program_map_configured = 1;
        return;
    }
    value = key_value_start(line, "fixed_size");
    if (value) {
        config->program_fixed_size = (uint32_t)strtoul(value, NULL, 0);
        config->program_map_configured = 1;
        return;
    }
    value = key_value_start(line, "bank_window_base");
    if (value) {
        config->program_bank_window_base = (uint32_t)strtoul(value, NULL, 0);
        config->program_map_configured = 1;
        return;
    }
    value = key_value_start(line, "bank_window_size");
    if (value) {
        config->program_bank_window_size = (uint32_t)strtoul(value, NULL, 0);
        config->program_map_configured = 1;
    }
}

static int ng_game_config_load_into(const char *path,
                                    NgGameConfig *config,
                                    unsigned depth) {
    if (!path || !config || depth > 8u) {
        return 0;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }

    NgGameConfigSection section = NG_GAME_CONFIG_SECTION_NONE;
    NgGameConfigArray array = NG_GAME_CONFIG_ARRAY_NONE;
    NgGameConfigJumpTable *current_jump_table = NULL;
    NgGameConfigTableCall *current_table_call = NULL;
    NgGameConfigStateTable *current_state_table = NULL;
    NgGameConfigRecordFormat *current_record_format = NULL;
    NgGameConfigRoutineTable *current_routine_table = NULL;
    NgGameConfigDispatcher *current_dispatcher = NULL;
    NgGameConfigBank *current_bank = NULL;
    NgGameConfigPathList discovery_files;
    memset(&discovery_files, 0, sizeof(discovery_files));
    char base_dir[NG_GAME_CONFIG_MAX_PATH];
    dirname_of(path, base_dir, sizeof(base_dir));
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char *hash = strchr(line, '#');
        if (hash) {
            *hash = '\0';
        }

        char *trimmed = trim_left(line);
        if (*trimmed == '\0') {
            continue;
        }

        if (*trimmed == '[') {
            if (strncmp(trimmed,
                        "[[jump_table]]",
                        sizeof("[[jump_table]]") - 1u) == 0) {
                section = NG_GAME_CONFIG_SECTION_JUMP_TABLE;
                current_jump_table = append_jump_table(config);
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed,
                               "[[table_call]]",
                               sizeof("[[table_call]]") - 1u) == 0) {
                section = NG_GAME_CONFIG_SECTION_TABLE_CALL;
                current_jump_table = NULL;
                current_table_call = append_table_call(config);
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed,
                               "[[state_table]]",
                               sizeof("[[state_table]]") - 1u) == 0) {
                section = NG_GAME_CONFIG_SECTION_STATE_TABLE;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = append_state_table(config);
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed,
                               "[[record_format]]",
                               sizeof("[[record_format]]") - 1u) == 0) {
                section = NG_GAME_CONFIG_SECTION_RECORD_FORMAT;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = append_record_format(config);
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed,
                               "[[routine_table]]",
                               sizeof("[[routine_table]]") - 1u) == 0) {
                section = NG_GAME_CONFIG_SECTION_ROUTINE_TABLE;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = append_routine_table(config);
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed,
                               "[[dispatcher]]",
                               sizeof("[[dispatcher]]") - 1u) == 0) {
                section = NG_GAME_CONFIG_SECTION_DISPATCHER;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = append_dispatcher(config);
                current_bank = NULL;
            } else if (strncmp(trimmed,
                               "[[bank]]",
                               sizeof("[[bank]]") - 1u) == 0) {
                section = NG_GAME_CONFIG_SECTION_BANK;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = append_bank(config);
            } else if (strncmp(trimmed, "[functions]", 11u) == 0) {
                section = NG_GAME_CONFIG_SECTION_FUNCTIONS;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed, "[program]", 9u) == 0) {
                section = NG_GAME_CONFIG_SECTION_PROGRAM;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed, "[dispatch]", 10u) == 0) {
                section = NG_GAME_CONFIG_SECTION_DISPATCH;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else if (strncmp(trimmed, "[game]", 6u) == 0) {
                section = NG_GAME_CONFIG_SECTION_GAME;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            } else {
                section = NG_GAME_CONFIG_SECTION_NONE;
                current_jump_table = NULL;
                current_table_call = NULL;
                current_state_table = NULL;
                current_record_format = NULL;
                current_routine_table = NULL;
                current_dispatcher = NULL;
                current_bank = NULL;
            }
            array = NG_GAME_CONFIG_ARRAY_NONE;
            continue;
        }

        if (section != NG_GAME_CONFIG_SECTION_FUNCTIONS &&
            section != NG_GAME_CONFIG_SECTION_GAME &&
            section != NG_GAME_CONFIG_SECTION_PROGRAM &&
            section != NG_GAME_CONFIG_SECTION_DISPATCH &&
            section != NG_GAME_CONFIG_SECTION_JUMP_TABLE &&
            section != NG_GAME_CONFIG_SECTION_TABLE_CALL &&
            section != NG_GAME_CONFIG_SECTION_STATE_TABLE &&
            section != NG_GAME_CONFIG_SECTION_RECORD_FORMAT &&
            section != NG_GAME_CONFIG_SECTION_ROUTINE_TABLE &&
            section != NG_GAME_CONFIG_SECTION_DISPATCHER &&
            section != NG_GAME_CONFIG_SECTION_BANK) {
            continue;
        }

        if (section == NG_GAME_CONFIG_SECTION_FUNCTIONS) {
            if (key_starts_array(trimmed, "entry")) {
                array = NG_GAME_CONFIG_ARRAY_ENTRY;
            } else if (key_starts_array(trimmed, "extra")) {
                array = NG_GAME_CONFIG_ARRAY_EXTRA;
            }
        } else if (section == NG_GAME_CONFIG_SECTION_GAME) {
            if (key_starts_array(trimmed, "discovery_files")) {
                array = NG_GAME_CONFIG_ARRAY_DISCOVERY_FILES;
            }
        } else if (section == NG_GAME_CONFIG_SECTION_PROGRAM) {
            parse_program_scalar(trimmed, config);
            continue;
        } else if (section == NG_GAME_CONFIG_SECTION_DISPATCH) {
            if (key_starts_array(trimmed, "runtime")) {
                array = NG_GAME_CONFIG_ARRAY_RUNTIME_DISPATCH;
            }
        } else if (section == NG_GAME_CONFIG_SECTION_JUMP_TABLE) {
            parse_jump_table_scalar(trimmed, current_jump_table);
            continue;
        } else if (section == NG_GAME_CONFIG_SECTION_TABLE_CALL) {
            parse_table_call_scalar(trimmed, current_table_call);
            continue;
        } else if (section == NG_GAME_CONFIG_SECTION_STATE_TABLE) {
            if (array == NG_GAME_CONFIG_ARRAY_STATE_TABLE_SCAN) {
                parse_state_table_scan_values(trimmed,
                                              config,
                                              current_state_table);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            if (key_starts_array(trimmed, "scan")) {
                array = NG_GAME_CONFIG_ARRAY_STATE_TABLE_SCAN;
                parse_state_table_scan_values(trimmed,
                                              config,
                                              current_state_table);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            parse_state_table_scalar(trimmed, current_state_table);
            continue;
        } else if (section == NG_GAME_CONFIG_SECTION_ROUTINE_TABLE) {
            if (array == NG_GAME_CONFIG_ARRAY_ROUTINE_TABLE_SCAN) {
                parse_routine_table_scan_values(trimmed,
                                                config,
                                                current_routine_table);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            if (key_starts_array(trimmed, "scan")) {
                array = NG_GAME_CONFIG_ARRAY_ROUTINE_TABLE_SCAN;
                parse_routine_table_scan_values(trimmed,
                                                config,
                                                current_routine_table);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            parse_routine_table_scalar(trimmed, current_routine_table);
            continue;
        } else if (section == NG_GAME_CONFIG_SECTION_DISPATCHER) {
            if (array == NG_GAME_CONFIG_ARRAY_DISPATCHER_INSTALL_SLOTS ||
                array == NG_GAME_CONFIG_ARRAY_DISPATCHER_SPAWN_HELPERS) {
                parse_dispatcher_array_values(trimmed,
                                              config,
                                              current_dispatcher,
                                              array);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            if (key_starts_array(trimmed, "install_slots")) {
                array = NG_GAME_CONFIG_ARRAY_DISPATCHER_INSTALL_SLOTS;
                parse_dispatcher_array_values(trimmed,
                                              config,
                                              current_dispatcher,
                                              array);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            if (key_starts_array(trimmed, "spawn_helpers")) {
                array = NG_GAME_CONFIG_ARRAY_DISPATCHER_SPAWN_HELPERS;
                parse_dispatcher_array_values(trimmed,
                                              config,
                                              current_dispatcher,
                                              array);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            parse_dispatcher_scalar(trimmed, current_dispatcher);
            continue;
        } else if (section == NG_GAME_CONFIG_SECTION_BANK) {
            parse_bank_scalar(trimmed, current_bank);
            continue;
        } else if (section == NG_GAME_CONFIG_SECTION_RECORD_FORMAT) {
            if (array == NG_GAME_CONFIG_ARRAY_RECORD_CALLBACK_OFFSETS) {
                parse_record_callback_offsets(trimmed,
                                              config,
                                              current_record_format);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            if (array == NG_GAME_CONFIG_ARRAY_RECORD_SCAN) {
                parse_record_scan_values(trimmed,
                                         config,
                                         current_record_format);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            if (key_starts_array(trimmed, "callback_offsets")) {
                array = NG_GAME_CONFIG_ARRAY_RECORD_CALLBACK_OFFSETS;
                parse_record_callback_offsets(trimmed,
                                              config,
                                              current_record_format);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            if (key_starts_array(trimmed, "scan")) {
                array = NG_GAME_CONFIG_ARRAY_RECORD_SCAN;
                parse_record_scan_values(trimmed,
                                         config,
                                         current_record_format);
                if (strchr(trimmed, ']')) {
                    array = NG_GAME_CONFIG_ARRAY_NONE;
                }
                continue;
            }
            parse_record_format_scalar(trimmed, current_record_format);
            continue;
        }

        if (array != NG_GAME_CONFIG_ARRAY_NONE) {
            if (array == NG_GAME_CONFIG_ARRAY_DISCOVERY_FILES) {
                parse_string_values(trimmed, config, &discovery_files);
            } else if (array == NG_GAME_CONFIG_ARRAY_RECORD_CALLBACK_OFFSETS) {
                parse_record_callback_offsets(trimmed,
                                              config,
                                              current_record_format);
            } else if (array == NG_GAME_CONFIG_ARRAY_RECORD_SCAN) {
                parse_record_scan_values(trimmed,
                                         config,
                                         current_record_format);
            } else if (array == NG_GAME_CONFIG_ARRAY_STATE_TABLE_SCAN) {
                parse_state_table_scan_values(trimmed,
                                              config,
                                              current_state_table);
            } else if (array == NG_GAME_CONFIG_ARRAY_ROUTINE_TABLE_SCAN) {
                parse_routine_table_scan_values(trimmed,
                                                config,
                                                current_routine_table);
            } else if (array == NG_GAME_CONFIG_ARRAY_DISPATCHER_INSTALL_SLOTS ||
                       array == NG_GAME_CONFIG_ARRAY_DISPATCHER_SPAWN_HELPERS) {
                parse_dispatcher_array_values(trimmed,
                                              config,
                                              current_dispatcher,
                                              array);
            } else {
                parse_address_values(trimmed, array, config);
            }
            if (strchr(trimmed, ']')) {
                array = NG_GAME_CONFIG_ARRAY_NONE;
            }
        }
    }

    int ok = ferror(f) == 0;
    fclose(f);
    if (!ok) {
        return 0;
    }

    for (uint32_t i = 0; i < discovery_files.count; ++i) {
        char child_path[NG_GAME_CONFIG_MAX_PATH];
        join_path(base_dir,
                  discovery_files.paths[i],
                  child_path,
                  sizeof(child_path));

        NgGameConfig child;
        ng_game_config_init(&child);
        if (!ng_game_config_load_into(child_path, &child, depth + 1u)) {
            return 0;
        }
        append_config_values(config, &child);
    }

    return ok;
}

int ng_game_config_load(const char *path, NgGameConfig *config) {
    if (!path || !config) {
        return 0;
    }

    ng_game_config_init(config);
    return ng_game_config_load_into(path, config, 0u);
}
