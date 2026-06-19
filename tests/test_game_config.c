#include "game_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
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

    char dir[] = "/tmp/neo-game-config-dir-XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char subdir[256];
    snprintf(subdir, sizeof(subdir), "%s/sub", dir);
    CHECK(mkdir(subdir, 0700) == 0);

    char child_a[256];
    snprintf(child_a, sizeof(child_a), "%s/seeds.toml", dir);
    out = fopen(child_a, "w");
    CHECK(out != NULL);
    fprintf(out,
            "[functions]\n"
            "entry = [0x000120]\n"
            "extra = [0x000130]\n");
    CHECK(fclose(out) == 0);

    char child_b[256];
    snprintf(child_b, sizeof(child_b), "%s/sub/more.toml", dir);
    out = fopen(child_b, "w");
    CHECK(out != NULL);
    fprintf(out,
            "[functions]\n"
            "extra = [0x000140]\n");
    CHECK(fclose(out) == 0);

    char parent[256];
    snprintf(parent, sizeof(parent), "%s/game.toml", dir);
    out = fopen(parent, "w");
    CHECK(out != NULL);
    fprintf(out,
            "[game]\n"
            "discovery_files = [\n"
            "  \"seeds.toml\",\n"
            "  \"sub/more.toml\",\n"
            "]\n"
            "\n"
            "[functions]\n"
            "entry = [0x000040]\n"
            "extra = [0x000100]\n");
    CHECK(fclose(out) == 0);

    CHECK(ng_game_config_load(parent, &config));
    CHECK(config.entry_count == 2u);
    CHECK(config.entry[0] == 0x000040u);
    CHECK(config.entry[1] == 0x000120u);
    CHECK(config.extra_count == 3u);
    CHECK(config.extra[0] == 0x000100u);
    CHECK(config.extra[1] == 0x000130u);
    CHECK(config.extra[2] == 0x000140u);
    CHECK(config.discovery_file_count == 2u);
    CHECK(!config.truncated);

    unlink(parent);
    unlink(child_a);
    unlink(child_b);
    rmdir(subdir);
    rmdir(dir);

    ng_game_config_init(&config);
    CHECK(config.entry_count == 0u);
    CHECK(config.extra_count == 0u);
    CHECK(config.discovery_file_count == 0u);
    CHECK(!config.truncated);

    return 0;
}
