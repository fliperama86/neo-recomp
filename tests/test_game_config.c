#include "game_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define NG_PATH_SEP "\\"
#define NG_MKDIR(path) _mkdir(path)
#define NG_RMDIR(path) _rmdir(path)
#else
#include <unistd.h>
#define NG_PATH_SEP "/"
#define NG_MKDIR(path) mkdir((path), 0700)
#define NG_RMDIR(path) rmdir(path)
#endif

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int open_temp_file(char *path, size_t path_size, FILE **out) {
    if (!path || path_size == 0u || !out) {
        return 0;
    }
    *out = NULL;

    for (unsigned i = 0; i < 10000u; ++i) {
        int len = snprintf(path, path_size, "neo-game-config-%u.toml", i);
        if (len < 0 || (size_t)len >= path_size) {
            return 0;
        }

        FILE *existing = fopen(path, "rb");
        if (existing) {
            fclose(existing);
            continue;
        }

        *out = fopen(path, "w");
        if (*out) {
            return 1;
        }
    }

    path[0] = '\0';
    return 0;
}

static int make_temp_dir(char *path, size_t path_size) {
    if (!path || path_size == 0u) {
        return 0;
    }

    for (unsigned i = 0; i < 10000u; ++i) {
        int len = snprintf(path, path_size, "neo-game-config-dir-%u", i);
        if (len < 0 || (size_t)len >= path_size) {
            return 0;
        }
        if (NG_MKDIR(path) == 0) {
            return 1;
        }
    }

    path[0] = '\0';
    return 0;
}

int main(void) {
    char path[256];
    FILE *out = NULL;
    CHECK(open_temp_file(path, sizeof(path), &out));
    fprintf(out,
            "[game]\n"
            "name = \"Synthetic\"\n"
            "\n"
            "[program]\n"
            "fixed_base = 0x000000\n"
            "fixed_size = 0x100000\n"
            "bank_window_base = 0x200000\n"
            "bank_window_size = 0x100000\n"
            "\n"
            "[[bank]]\n"
            "id = 2\n"
            "offset = 0x300000\n"
            "size = 0x080000\n"
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
    remove(path);

    CHECK(config.program_map_configured);
    CHECK(config.program_fixed_base == 0x000000u);
    CHECK(config.program_fixed_size == 0x100000u);
    CHECK(config.program_bank_window_base == 0x200000u);
    CHECK(config.program_bank_window_size == 0x100000u);
    CHECK(config.bank_count == 1u);
    CHECK(config.banks[0].id == 2u);
    CHECK(config.banks[0].offset == 0x300000u);
    CHECK(config.banks[0].size == 0x080000u);
    CHECK(config.banks[0].has_offset);
    CHECK(config.banks[0].has_size);
    CHECK(config.entry_count == 2u);
    CHECK(config.entry[0] == 0x000040u);
    CHECK(config.entry[1] == 0x000080u);
    CHECK(config.extra_count == 2u);
    CHECK(config.extra[0] == 0x000100u);
    CHECK(config.extra[1] == 512u);
    CHECK(!config.truncated);

    char dir[256];
    CHECK(make_temp_dir(dir, sizeof(dir)));
    char subdir[256];
    snprintf(subdir, sizeof(subdir), "%s" NG_PATH_SEP "sub", dir);
    CHECK(NG_MKDIR(subdir) == 0);

    char child_a[256];
    snprintf(child_a, sizeof(child_a), "%s" NG_PATH_SEP "seeds.toml", dir);
    out = fopen(child_a, "w");
    CHECK(out != NULL);
    fprintf(out,
            "[functions]\n"
            "entry = [0x000120]\n"
            "extra = [0x000130]\n"
            "\n"
            "[dispatch]\n"
            "runtime = [0x0001C0]\n"
            "\n"
            "[[jump_table]]\n"
            "start = 0x000190\n"
            "end = 0x000194\n"
            "stride = 2\n"
            "format = \"pcrel16\"\n"
            "\n"
            "[[table_call]]\n"
            "helper = 0x000772\n"
            "table_reg = \"A0\"\n"
            "format = \"abs32_sparse\"\n"
            "sentinel = 0xFFFFFFFF\n"
            "table_start = 0x000100\n"
            "table_end = 0x000200\n"
            "max_entries = 12\n"
            "\n"
            "[[state_table]]\n"
            "root = 0x000140\n"
            "table_start = 0x000140\n"
            "table_end = 0x000180\n"
            "target = \"0x000300-0x000400\"\n"
            "follow_chain = false\n"
            "scan = [\"bank:1\"]\n"
            "\n"
            "[[record_format]]\n"
            "name = \"child_fixed\"\n"
            "width = 0x0A\n"
            "callback_offsets = [4, 6]\n"
            "sentinel = 0xFFFFFFFF\n"
            "target = \"0x000700-0x000800\"\n"
            "scan = [\"fixed\"]\n"
            "\n"
            "[[routine_table]]\n"
            "name = \"child_routine_stubs\"\n"
            "stride = 0x0E\n"
            "width = 0x0C\n"
            "min_instructions = 3\n"
            "scan = [\"bank:1\"]\n"
            "\n"
            "[[dispatcher]]\n"
            "kind = \"object_state\"\n"
            "install_slots = [0x3C]\n"
            "spawn_helpers = [\n"
            "  0x000100,\n"
            "]\n");
    CHECK(fclose(out) == 0);

    char child_b[256];
    snprintf(child_b,
             sizeof(child_b),
             "%s" NG_PATH_SEP "sub" NG_PATH_SEP "more.toml",
             dir);
    out = fopen(child_b, "w");
    CHECK(out != NULL);
    fprintf(out,
            "[functions]\n"
            "extra = [0x000140]\n");
    CHECK(fclose(out) == 0);

    char parent[256];
    snprintf(parent, sizeof(parent), "%s" NG_PATH_SEP "game.toml", dir);
    out = fopen(parent, "w");
    CHECK(out != NULL);
    fprintf(out,
            "[game]\n"
            "discovery_files = [\n"
            "  \"seeds.toml\",\n"
            "  \"sub/more.toml\",\n"
            "]\n"
            "\n"
            "[[jump_table]]\n"
            "start = 0x000180\n"
            "end = 0x000188\n"
            "stride = 4\n"
            "format = \"abs32\"\n"
            "\n"
            "[[jump_table]]\n"
            "start = 0x0001D0\n"
            "end = 0x0001E0\n"
            "stride = 2\n"
            "format = \"script_predicate\"\n"
            "\n"
            "[[jump_table]]\n"
            "start = 0x000220\n"
            "end = 0x000240\n"
            "stride = 2\n"
            "format = \"tagged_abs32\"\n"
            "match = 0x0800\n"
            "target_offset = 2\n"
            "target_start = 0x000300\n"
            "target_end = 0x000400\n"
            "\n"
            "[[jump_table]]\n"
            "start = 0x000260\n"
            "end = 0x000280\n"
            "stride = 2\n"
            "format = \"inline_callback\"\n"
            "target_offset = 2\n"
            "\n"
            "[[table_call]]\n"
            "helper = 0x028CD4\n"
            "table_reg = \"A0\"\n"
            "format = \"tagged_abs32\"\n"
            "match = 0x0800\n"
            "target_offset = 2\n"
            "target_start = 0x000300\n"
            "target_end = 0x000500\n"
            "table_start = 0x000200\n"
            "table_end = 0x000300\n"
            "stride = 2\n"
            "max_entries = 24\n"
            "\n"
            "[[state_table]]\n"
            "root = 0x0002A0\n"
            "table_start = 0x000280\n"
            "table_end = 0x000300\n"
            "stride = 4\n"
            "sentinel = 0xFFFFFFFF\n"
            "follow_chain = true\n"
            "skip_invalid_targets = true\n"
            "target_start = 0x000500\n"
            "target_end = 0x000600\n"
            "max_tables = 8\n"
            "max_entries = 16\n"
            "\n"
            "[[record_format]]\n"
            "name = \"tagged_stream\"\n"
            "tag = 0x0800\n"
            "tag_offset = 0\n"
            "stride = 2\n"
            "callback_offsets = [\n"
            "  2,\n"
            "]\n"
            "target_start = 0x000800\n"
            "target_end = 0x000900\n"
            "scan = [\n"
            "  \"0x000400-0x000480\",\n"
            "  \"bank:*\",\n"
            "  \"bank:2\",\n"
            "]\n"
            "\n"
            "[[routine_table]]\n"
            "name = \"fixed_routine_stubs\"\n"
            "stride = 0x0E\n"
            "width = 0x0C\n"
            "min_instructions = 3\n"
            "fallthrough_target = 0x000530\n"
            "scan = [\n"
            "  \"0x000500-0x000540\",\n"
            "  \"fixed\",\n"
            "]\n"
            "\n"
            "[[dispatcher]]\n"
            "kind = \"object_state\"\n"
            "state_slot = 0x70\n"
            "install_slots = [0x00, 0x70]\n"
            "spawn_helpers = [0x0004AE, 0x0006FE]\n"
            "\n"
            "[functions]\n"
            "entry = [0x000040]\n"
            "extra = [0x000100]\n"
            "\n"
            "[dispatch]\n"
            "runtime = [0x0001B0]\n");
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
    CHECK(config.runtime_dispatch_count == 2u);
    CHECK(config.runtime_dispatch[0] == 0x0001B0u);
    CHECK(config.runtime_dispatch[1] == 0x0001C0u);
    CHECK(config.jump_table_count == 5u);
    CHECK(config.jump_tables[0].start == 0x000180u);
    CHECK(config.jump_tables[0].end == 0x000188u);
    CHECK(config.jump_tables[0].stride == 4u);
    CHECK(config.jump_tables[0].format == NG_GAME_CONFIG_JUMP_TABLE_ABS32);
    CHECK(config.jump_tables[1].start == 0x0001D0u);
    CHECK(config.jump_tables[1].end == 0x0001E0u);
    CHECK(config.jump_tables[1].stride == 2u);
    CHECK(config.jump_tables[1].format ==
          NG_GAME_CONFIG_JUMP_TABLE_SCRIPT_PREDICATE);
    CHECK(config.jump_tables[2].start == 0x000220u);
    CHECK(config.jump_tables[2].end == 0x000240u);
    CHECK(config.jump_tables[2].stride == 2u);
    CHECK(config.jump_tables[2].format ==
          NG_GAME_CONFIG_JUMP_TABLE_TAGGED_ABS32);
    CHECK(config.jump_tables[2].match == 0x0800u);
    CHECK(config.jump_tables[2].target_offset == 2u);
    CHECK(config.jump_tables[2].target_start == 0x000300u);
    CHECK(config.jump_tables[2].target_end == 0x000400u);
    CHECK(config.jump_tables[3].start == 0x000260u);
    CHECK(config.jump_tables[3].end == 0x000280u);
    CHECK(config.jump_tables[3].stride == 2u);
    CHECK(config.jump_tables[3].format ==
          NG_GAME_CONFIG_JUMP_TABLE_INLINE_CALLBACK);
    CHECK(config.jump_tables[3].target_offset == 2u);
    CHECK(config.jump_tables[4].start == 0x000190u);
    CHECK(config.jump_tables[4].end == 0x000194u);
    CHECK(config.jump_tables[4].stride == 2u);
    CHECK(config.jump_tables[4].format == NG_GAME_CONFIG_JUMP_TABLE_PCREL16);
    CHECK(config.table_call_count == 2u);
    CHECK(config.table_calls[0].helper == 0x028CD4u);
    CHECK(config.table_calls[0].table_reg == 0u);
    CHECK(config.table_calls[0].format ==
          NG_GAME_CONFIG_TABLE_CALL_TAGGED_ABS32);
    CHECK(config.table_calls[0].match == 0x0800u);
    CHECK(config.table_calls[0].target_offset == 2u);
    CHECK(config.table_calls[0].target_start == 0x000300u);
    CHECK(config.table_calls[0].target_end == 0x000500u);
    CHECK(config.table_calls[0].table_start == 0x000200u);
    CHECK(config.table_calls[0].table_end == 0x000300u);
    CHECK(config.table_calls[0].stride == 2u);
    CHECK(config.table_calls[0].max_entries == 24u);
    CHECK(config.table_calls[1].helper == 0x000772u);
    CHECK(config.table_calls[1].table_reg == 0u);
    CHECK(config.table_calls[1].format ==
          NG_GAME_CONFIG_TABLE_CALL_ABS32_SPARSE);
    CHECK(config.table_calls[1].sentinel == 0xFFFFFFFFu);
    CHECK(config.table_calls[1].table_start == 0x000100u);
    CHECK(config.table_calls[1].table_end == 0x000200u);
    CHECK(config.table_calls[1].max_entries == 12u);
    CHECK(config.state_table_count == 2u);
    CHECK(config.state_tables[0].root == 0x0002A0u);
    CHECK(config.state_tables[0].table_start == 0x000280u);
    CHECK(config.state_tables[0].table_end == 0x000300u);
    CHECK(config.state_tables[0].stride == 4u);
    CHECK(config.state_tables[0].sentinel == 0xFFFFFFFFu);
    CHECK(config.state_tables[0].follow_chain);
    CHECK(config.state_tables[0].skip_invalid_targets);
    CHECK(config.state_tables[0].target_start == 0x000500u);
    CHECK(config.state_tables[0].target_end == 0x000600u);
    CHECK(config.state_tables[0].max_tables == 8u);
    CHECK(config.state_tables[0].max_entries == 16u);
    CHECK(config.state_tables[1].root == 0x000140u);
    CHECK(config.state_tables[1].table_start == 0x000140u);
    CHECK(config.state_tables[1].table_end == 0x000180u);
    CHECK(config.state_tables[1].stride == 4u);
    CHECK(config.state_tables[1].sentinel == 0xFFFFFFFFu);
    CHECK(!config.state_tables[1].follow_chain);
    CHECK(!config.state_tables[1].skip_invalid_targets);
    CHECK(config.state_tables[1].target_start == 0x000300u);
    CHECK(config.state_tables[1].target_end == 0x000400u);
    CHECK(config.state_tables[1].max_tables == 64u);
    CHECK(config.state_tables[1].max_entries == 1024u);
    CHECK(config.state_tables[1].scan_count == 1u);
    CHECK(config.state_tables[1].scans[0].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE);
    CHECK(config.state_tables[1].scans[0].bank_id == 1u);
    CHECK(config.record_format_count == 2u);
    CHECK(strcmp(config.record_formats[0].name, "tagged_stream") == 0);
    CHECK(config.record_formats[0].has_tag);
    CHECK(config.record_formats[0].tag == 0x0800u);
    CHECK(config.record_formats[0].tag_offset == 0u);
    CHECK(config.record_formats[0].stride == 2u);
    CHECK(config.record_formats[0].callback_offset_count == 1u);
    CHECK(config.record_formats[0].callback_offsets[0] == 2u);
    CHECK(!config.record_formats[0].has_sentinel);
    CHECK(config.record_formats[0].target_start == 0x000800u);
    CHECK(config.record_formats[0].target_end == 0x000900u);
    CHECK(config.record_formats[0].scan_count == 3u);
    CHECK(config.record_formats[0].scans[0].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_RANGE);
    CHECK(config.record_formats[0].scans[0].start == 0x000400u);
    CHECK(config.record_formats[0].scans[0].end == 0x000480u);
    CHECK(config.record_formats[0].scans[1].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL);
    CHECK(config.record_formats[0].scans[2].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE);
    CHECK(config.record_formats[0].scans[2].bank_id == 2u);
    CHECK(strcmp(config.record_formats[1].name, "child_fixed") == 0);
    CHECK(!config.record_formats[1].has_tag);
    CHECK(config.record_formats[1].width == 0x0Au);
    CHECK(config.record_formats[1].stride == 0u);
    CHECK(config.record_formats[1].callback_offset_count == 2u);
    CHECK(config.record_formats[1].callback_offsets[0] == 4u);
    CHECK(config.record_formats[1].callback_offsets[1] == 6u);
    CHECK(config.record_formats[1].has_sentinel);
    CHECK(config.record_formats[1].sentinel == 0xFFFFFFFFu);
    CHECK(config.record_formats[1].target_start == 0x000700u);
    CHECK(config.record_formats[1].target_end == 0x000800u);
    CHECK(config.record_formats[1].scan_count == 1u);
    CHECK(config.record_formats[1].scans[0].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_FIXED);
    CHECK(config.routine_table_count == 2u);
    CHECK(strcmp(config.routine_tables[0].name, "fixed_routine_stubs") == 0);
    CHECK(config.routine_tables[0].stride == 0x0Eu);
    CHECK(config.routine_tables[0].width == 0x0Cu);
    CHECK(config.routine_tables[0].min_instructions == 3u);
    CHECK(config.routine_tables[0].has_fallthrough_target);
    CHECK(config.routine_tables[0].fallthrough_target == 0x000530u);
    CHECK(config.routine_tables[0].scan_count == 2u);
    CHECK(config.routine_tables[0].scans[0].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_RANGE);
    CHECK(config.routine_tables[0].scans[0].start == 0x000500u);
    CHECK(config.routine_tables[0].scans[0].end == 0x000540u);
    CHECK(config.routine_tables[0].scans[1].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_FIXED);
    CHECK(strcmp(config.routine_tables[1].name, "child_routine_stubs") == 0);
    CHECK(config.routine_tables[1].stride == 0x0Eu);
    CHECK(config.routine_tables[1].width == 0x0Cu);
    CHECK(config.routine_tables[1].min_instructions == 3u);
    CHECK(!config.routine_tables[1].has_fallthrough_target);
    CHECK(config.routine_tables[1].scan_count == 1u);
    CHECK(config.routine_tables[1].scans[0].kind ==
          NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE);
    CHECK(config.routine_tables[1].scans[0].bank_id == 1u);
    CHECK(config.dispatcher_count == 2u);
    CHECK(config.dispatchers[0].kind ==
          NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE);
    CHECK(config.dispatchers[0].has_state_slot);
    CHECK(config.dispatchers[0].state_slot == 0x70u);
    CHECK(config.dispatchers[0].install_slot_count == 2u);
    CHECK(config.dispatchers[0].install_slots[0] == 0x00u);
    CHECK(config.dispatchers[0].install_slots[1] == 0x70u);
    CHECK(config.dispatchers[0].spawn_helper_count == 2u);
    CHECK(config.dispatchers[0].spawn_helpers[0] == 0x0004AEu);
    CHECK(config.dispatchers[0].spawn_helpers[1] == 0x0006FEu);
    CHECK(config.dispatchers[1].kind ==
          NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE);
    CHECK(!config.dispatchers[1].has_state_slot);
    CHECK(config.dispatchers[1].install_slot_count == 1u);
    CHECK(config.dispatchers[1].install_slots[0] == 0x3Cu);
    CHECK(config.dispatchers[1].spawn_helper_count == 1u);
    CHECK(config.dispatchers[1].spawn_helpers[0] == 0x000100u);
    CHECK(!config.truncated);

    remove(parent);
    remove(child_a);
    remove(child_b);
    NG_RMDIR(subdir);
    NG_RMDIR(dir);

    ng_game_config_init(&config);
    CHECK(config.entry_count == 0u);
    CHECK(config.extra_count == 0u);
    CHECK(config.discovery_file_count == 0u);
    CHECK(config.runtime_dispatch_count == 0u);
    CHECK(config.jump_table_count == 0u);
    CHECK(config.table_call_count == 0u);
    CHECK(config.state_table_count == 0u);
    CHECK(config.record_format_count == 0u);
    CHECK(config.routine_table_count == 0u);
    CHECK(config.dispatcher_count == 0u);
    CHECK(!config.program_map_configured);
    CHECK(!config.truncated);

    return 0;
}
