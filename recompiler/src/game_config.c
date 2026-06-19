#include "game_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum NgGameConfigSection {
    NG_GAME_CONFIG_SECTION_NONE,
    NG_GAME_CONFIG_SECTION_FUNCTIONS,
} NgGameConfigSection;

typedef enum NgGameConfigArray {
    NG_GAME_CONFIG_ARRAY_NONE,
    NG_GAME_CONFIG_ARRAY_ENTRY,
    NG_GAME_CONFIG_ARRAY_EXTRA,
} NgGameConfigArray;

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

int ng_game_config_load(const char *path, NgGameConfig *config) {
    if (!path || !config) {
        return 0;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }

    ng_game_config_init(config);
    NgGameConfigSection section = NG_GAME_CONFIG_SECTION_NONE;
    NgGameConfigArray array = NG_GAME_CONFIG_ARRAY_NONE;
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
            section = strncmp(trimmed, "[functions]", 11u) == 0 ?
                NG_GAME_CONFIG_SECTION_FUNCTIONS :
                NG_GAME_CONFIG_SECTION_NONE;
            array = NG_GAME_CONFIG_ARRAY_NONE;
            continue;
        }

        if (section != NG_GAME_CONFIG_SECTION_FUNCTIONS) {
            continue;
        }

        if (key_starts_array(trimmed, "entry")) {
            array = NG_GAME_CONFIG_ARRAY_ENTRY;
        } else if (key_starts_array(trimmed, "extra")) {
            array = NG_GAME_CONFIG_ARRAY_EXTRA;
        }

        if (array != NG_GAME_CONFIG_ARRAY_NONE) {
            parse_address_values(trimmed, array, config);
            if (strchr(trimmed, ']')) {
                array = NG_GAME_CONFIG_ARRAY_NONE;
            }
        }
    }

    int ok = ferror(f) == 0;
    fclose(f);
    return ok;
}
