#include <stdint.h>
#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

void ng_generated_call(uint32_t addr);
void ng_generated_smoke_reset_dispatch_stats(void);
void ng_generated_smoke_set_dispatch_budget(uint64_t max_dispatches);
uint64_t ng_generated_smoke_dispatch_count(void);
uint64_t ng_generated_smoke_cart_dispatch_count(void);
uint64_t ng_generated_smoke_bios_dispatch_count(void);
int ng_generated_smoke_dispatch_budget_hit(void);
uint32_t ng_generated_smoke_dispatch_budget_stop_addr(void);
uint32_t ng_generated_smoke_recent_loop_period(void);

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
    } else if ((addr & 0x00FFFFFFu) == 0x00000200u) {
        ng_generated_call(0x00000202u);
    } else if ((addr & 0x00FFFFFFu) == 0x00000202u) {
        ng_generated_call(0x00000200u);
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

static void reset_test_counters(void) {
    g_cart_calls = 0;
    g_bios_calls = 0;
    g_cart_depth = 0;
    g_bios_depth = 0;
    g_max_cart_depth = 0;
    g_max_bios_depth = 0;
    g_last_cart_addr = 0;
}

int main(void) {
    ng_generated_smoke_reset_dispatch_stats();
    ng_generated_call(0x00000100u);

    CHECK(g_bios_calls == 1u);
    CHECK(g_cart_calls == 33u);
    CHECK(g_last_cart_addr == 0x00000140u);
    CHECK(g_max_cart_depth == 1u);
    CHECK(g_max_bios_depth == 1u);
    CHECK(g_cart_depth == 0u);
    CHECK(g_bios_depth == 0u);
    CHECK(ng_generated_smoke_dispatch_count() == 34u);
    CHECK(ng_generated_smoke_cart_dispatch_count() == 33u);
    CHECK(ng_generated_smoke_bios_dispatch_count() == 1u);
    CHECK(!ng_generated_smoke_dispatch_budget_hit());
    CHECK(ng_generated_smoke_recent_loop_period() == 0u);

    reset_test_counters();
    ng_generated_smoke_reset_dispatch_stats();
    ng_generated_smoke_set_dispatch_budget(4u);
    ng_generated_call(0x00000100u);

    CHECK(g_bios_calls == 1u);
    CHECK(g_cart_calls == 3u);
    CHECK(g_last_cart_addr == 0x00000104u);
    CHECK(g_max_cart_depth == 1u);
    CHECK(g_max_bios_depth == 1u);
    CHECK(g_cart_depth == 0u);
    CHECK(g_bios_depth == 0u);
    CHECK(ng_generated_smoke_dispatch_count() == 4u);
    CHECK(ng_generated_smoke_cart_dispatch_count() == 3u);
    CHECK(ng_generated_smoke_bios_dispatch_count() == 1u);
    CHECK(ng_generated_smoke_dispatch_budget_hit());
    CHECK(ng_generated_smoke_dispatch_budget_stop_addr() == 0x00000106u);
    CHECK(ng_generated_smoke_recent_loop_period() == 0u);

    reset_test_counters();
    ng_generated_smoke_reset_dispatch_stats();
    ng_generated_smoke_set_dispatch_budget(8u);
    ng_generated_call(0x00000200u);

    CHECK(g_cart_calls == 8u);
    CHECK(g_bios_calls == 0u);
    CHECK(g_last_cart_addr == 0x00000202u);
    CHECK(ng_generated_smoke_dispatch_count() == 8u);
    CHECK(ng_generated_smoke_cart_dispatch_count() == 8u);
    CHECK(ng_generated_smoke_bios_dispatch_count() == 0u);
    CHECK(ng_generated_smoke_dispatch_budget_hit());
    CHECK(ng_generated_smoke_dispatch_budget_stop_addr() == 0x00000200u);
    CHECK(ng_generated_smoke_recent_loop_period() == 2u);

    ng_generated_smoke_set_dispatch_budget(0u);
    return 0;
}
