#pragma once

#include "function_discovery.h"
#include "m68k_decode.h"

#include <stdio.h>
#include <stdint.h>

#define NG_DISPATCH_AUDIT_MAX_SITES NG_FUNCTION_DISCOVERY_MAX_CANDIDATES

typedef enum NgDispatchAuditKind {
    NG_DISPATCH_AUDIT_DIRECT,
    NG_DISPATCH_AUDIT_COMPUTED,
    NG_DISPATCH_AUDIT_JUMP_TABLE,
} NgDispatchAuditKind;

typedef struct NgDispatchAuditSite {
    NgDispatchAuditKind kind;
    uint32_t site_addr;
    uint32_t site_bank;
    uint32_t target_addr;
    uint32_t target_bank;
    uint32_t table_addr;
    uint32_t table_bank;
    uint8_t mnemonic;
    uint8_t site_banked;
    uint8_t target_banked;
    uint8_t table_banked;
    uint8_t target_known;
    uint8_t target_in_discovery;
    uint8_t target_external;
    uint8_t runtime_allowed;
    uint8_t resolved_entries;
    uint8_t missing_entries;
} NgDispatchAuditSite;

typedef struct NgDispatchAudit {
    NgDispatchAuditSite sites[NG_DISPATCH_AUDIT_MAX_SITES];
    uint32_t count;
    uint32_t direct_count;
    uint32_t missing_direct_count;
    uint32_t external_direct_count;
    uint32_t computed_count;
    uint32_t runtime_computed_count;
    uint32_t jump_table_count;
    uint32_t jump_table_resolved_entries;
    uint32_t jump_table_missing_entries;
    int truncated;
} NgDispatchAudit;

void ng_dispatch_audit_init(NgDispatchAudit *audit);
int ng_dispatch_audit_build(const NgProgramRom *rom,
                            const NgFunctionDiscovery *discovery,
                            NgDispatchAudit *audit);
int ng_dispatch_audit_build_with_config(const NgProgramRom *rom,
                                        const NgFunctionDiscovery *discovery,
                                        const NgGameConfig *config,
                                        NgDispatchAudit *audit);
int ng_dispatch_audit_has_gaps(const NgDispatchAudit *audit);
int ng_dispatch_audit_write(FILE *out, const NgDispatchAudit *audit);
int ng_dispatch_audit_write_suggestions(FILE *out,
                                        const NgDispatchAudit *audit);
