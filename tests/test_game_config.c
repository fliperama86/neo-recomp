#include "game_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    char path[] = "/tmp/neo-game-config-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    FILE *out = fdopen(fd, "w");
    CHECK(out != NULL);
    fprintf(out,
            "[game]\n"
            "name = \"Synthetic\"\n"
            "\n"
            "[functions]\n"
            "entry = [\n"
            "  0x000040,\n"
            "  0x000080, # comment\n"
            "]\n"
            "extra = [ 0x000100, 512 ]\n");
    CHECK(fclose(out) == 0);

    NgGameConfig config;
    CHECK(ng_game_config_load(path, &config));
    unlink(path);

    CHECK(config.entry_count == 2u);
    CHECK(config.entry[0] == 0x000040u);
    CHECK(config.entry[1] == 0x000080u);
    CHECK(config.extra_count == 2u);
    CHECK(config.extra[0] == 0x000100u);
    CHECK(config.extra[1] == 512u);
    CHECK(!config.truncated);

    ng_game_config_init(&config);
    CHECK(config.entry_count == 0u);
    CHECK(config.extra_count == 0u);
    CHECK(!config.truncated);

    return 0;
}
