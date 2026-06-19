#include <stdint.h>
#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

void ng_generated_call(uint32_t addr);

static uint32_t g_cart_calls;
static uint32_t g_bios_calls;
static uint32_t g_cart_depth;
static uint32_t g_bios_depth;
static uint32_t g_max_cart_depth;
static uint32_t g_max_bios_depth;
static uint32_t g_last_cart_addr;

void ng_cart_generated_call(uint32_t addr) {
    ++g_cart_depth;
    if (g_cart_depth > g_max_cart_depth) {
        g_max_cart_depth = g_cart_depth;
    }
    ++g_cart_calls;
    g_last_cart_addr = addr & 0x00FFFFFFu;

    if ((addr & 0x00FFFFFFu) == 0x00000100u) {
        ng_generated_call(0x00C00000u);
    } else if ((addr & 0x00FFFFFFu) < 0x00000140u) {
        ng_generated_call((addr + 2u) & 0x00FFFFFFu);
    }

    --g_cart_depth;
}

void ng_bios_generated_call(uint32_t addr) {
    ++g_bios_depth;
    if (g_bios_depth > g_max_bios_depth) {
        g_max_bios_depth = g_bios_depth;
    }
    ++g_bios_calls;

    if ((addr & 0x00FFFFFFu) == 0x00C00000u) {
        ng_generated_call(0x00000102u);
    }

    --g_bios_depth;
}

int main(void) {
    ng_generated_call(0x00000100u);

    CHECK(g_bios_calls == 1u);
    CHECK(g_cart_calls == 33u);
    CHECK(g_last_cart_addr == 0x00000140u);
    CHECK(g_max_cart_depth == 1u);
    CHECK(g_max_bios_depth == 1u);
    CHECK(g_cart_depth == 0u);
    CHECK(g_bios_depth == 0u);
    return 0;
}
