#define _CRT_SECURE_NO_WARNINGS

#include "c_emitter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int read_file(FILE *f, char *out, size_t out_size) {
    long size;
    rewind(f);
    if (fseek(f, 0, SEEK_END) != 0) {
        return 0;
    }
    size = ftell(f);
    if (size < 0 || (size_t)size >= out_size) {
        return 0;
    }
    rewind(f);
    if (fread(out, 1, (size_t)size, f) != (size_t)size) {
        return 0;
    }
    out[size] = 0;
    return 1;
}

int main(void) {
    char symbol[32];
    NgFunctionDiscovery discovery;
    FILE *out;
    char text[4096];

    ng_c_symbol_for_addr(0x000007CCu, symbol, (unsigned)sizeof(symbol));
    CHECK(strcmp(symbol, "ng_func_0007CC") == 0);

    ng_function_discovery_init(&discovery);
    discovery.addrs[discovery.count++] = 0x000007CCu;
    discovery.addrs[discovery.count++] = 0x0000080Cu;
    discovery.addrs[discovery.count++] = 0x00024E38u;

    out = tmpfile();
    CHECK(out != NULL);
    CHECK(ng_emit_c_skeleton(out, &discovery));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);

    CHECK(strstr(text, "#include \"ngrecomp/neogeo_runtime.h\"") != NULL);
    CHECK(strstr(text, "static void ng_func_0007CC(void);") != NULL);
    CHECK(strstr(text, "case 0x000007CCu: ng_func_0007CC(); return;") != NULL);
    CHECK(strstr(text, "static void ng_func_024E38(void)") != NULL);
    CHECK(strstr(text, "ng_log_dispatch_miss(0x00024E38u);") != NULL);

    return 0;
}
