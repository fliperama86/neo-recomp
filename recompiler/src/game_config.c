#include "game_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum NgGameConfigSection {
    NG_GAME_CONFIG_SECTION_NONE,
    NG_GAME_CONFIG_SECTION_GAME,
    NG_GAME_CONFIG_SECTION_FUNCTIONS,
    NG_GAME_CONFIG_SECTION_DISPATCH,
    NG_GAME_CONFIG_SECTION_JUMP_TABLE,
} NgGameConfigSection;

typedef enum NgGameConfigArray {
    NG_GAME_CONFIG_ARRAY_NONE,
    NG_GAME_CONFIG_ARRAY_ENTRY,
    NG_GAME_CONFIG_ARRAY_EXTRA,
    NG_GAME_CONFIG_ARRAY_DISCOVERY_FILES,
    NG_GAME_CONFIG_ARRAY_RUNTIME_DISPATCH,
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
    value = key_value_start(line, "format");
    if (value) {
        parse_jump_table_format(value, table);
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
            } else if (strncmp(trimmed, "[functions]", 11u) == 0) {
                section = NG_GAME_CONFIG_SECTION_FUNCTIONS;
                current_jump_table = NULL;
            } else if (strncmp(trimmed, "[dispatch]", 10u) == 0) {
                section = NG_GAME_CONFIG_SECTION_DISPATCH;
                current_jump_table = NULL;
            } else if (strncmp(trimmed, "[game]", 6u) == 0) {
                section = NG_GAME_CONFIG_SECTION_GAME;
                current_jump_table = NULL;
            } else {
                section = NG_GAME_CONFIG_SECTION_NONE;
                current_jump_table = NULL;
            }
            array = NG_GAME_CONFIG_ARRAY_NONE;
            continue;
        }

        if (section != NG_GAME_CONFIG_SECTION_FUNCTIONS &&
            section != NG_GAME_CONFIG_SECTION_GAME &&
            section != NG_GAME_CONFIG_SECTION_DISPATCH &&
            section != NG_GAME_CONFIG_SECTION_JUMP_TABLE) {
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
        } else if (section == NG_GAME_CONFIG_SECTION_DISPATCH) {
            if (key_starts_array(trimmed, "runtime")) {
                array = NG_GAME_CONFIG_ARRAY_RUNTIME_DISPATCH;
            }
        } else if (section == NG_GAME_CONFIG_SECTION_JUMP_TABLE) {
            parse_jump_table_scalar(trimmed, current_jump_table);
            continue;
        }

        if (array != NG_GAME_CONFIG_ARRAY_NONE) {
            if (array == NG_GAME_CONFIG_ARRAY_DISCOVERY_FILES) {
                parse_string_values(trimmed, config, &discovery_files);
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
