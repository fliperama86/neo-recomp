#include "game_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum NgGameConfigSection {
    NG_GAME_CONFIG_SECTION_NONE,
    NG_GAME_CONFIG_SECTION_GAME,
    NG_GAME_CONFIG_SECTION_FUNCTIONS,
} NgGameConfigSection;

typedef enum NgGameConfigArray {
    NG_GAME_CONFIG_ARRAY_NONE,
    NG_GAME_CONFIG_ARRAY_ENTRY,
    NG_GAME_CONFIG_ARRAY_EXTRA,
    NG_GAME_CONFIG_ARRAY_DISCOVERY_FILES,
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

static void dirname_of(const char *path, char *out, size_t out_size) {
    if (!out || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (!path || !*path) {
        snprintf(out, out_size, ".");
        return;
    }

    const char *slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out, out_size, ".");
        return;
    }
    size_t len = (size_t)(slash - path);
    if (len == 0u) {
        len = 1u;
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
    if (path[0] == '/') {
        snprintf(out, out_size, "%s", path);
    } else if (!base || !*base || strcmp(base, ".") == 0) {
        snprintf(out, out_size, "%s", path);
    } else {
        snprintf(out, out_size, "%s/%s", base, path);
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
    } else {
        return;
    }

    if (*count >= NG_GAME_CONFIG_MAX_FUNCTIONS) {
        config->truncated = 1;
        return;
    }
    items[(*count)++] = value;
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
    if (src->truncated) {
        dst->truncated = 1;
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
            if (strncmp(trimmed, "[functions]", 11u) == 0) {
                section = NG_GAME_CONFIG_SECTION_FUNCTIONS;
            } else if (strncmp(trimmed, "[game]", 6u) == 0) {
                section = NG_GAME_CONFIG_SECTION_GAME;
            } else {
                section = NG_GAME_CONFIG_SECTION_NONE;
            }
            array = NG_GAME_CONFIG_ARRAY_NONE;
            continue;
        }

        if (section != NG_GAME_CONFIG_SECTION_FUNCTIONS &&
            section != NG_GAME_CONFIG_SECTION_GAME) {
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
