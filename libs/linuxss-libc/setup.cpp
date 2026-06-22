/**
 * @file setup.cpp
 * @author theflysong
 * @brief linux subsystem libc runtime bootstrap restore
 * @version alpha-1.0.0
 * @date 2026-06-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <prm.h>

#include <cstring>

size_t __linuxss_ssheap_base   = 0;
size_t __linuxss_ss_brk        = 0;
CapIdx __linuxss_ssheap_mem_cap = cap::null;
size_t __linuxss_bsargc        = 0;
const bsheader **__linuxss_bsargv = nullptr;

namespace {
    bool has_memory_kind(const char *desc, const char *kind) {
        if (desc == nullptr || kind == nullptr || desc[0] != '#') {
            return false;
        }
        ++desc;
        size_t kind_len = strlen(kind);
        return strncmp(desc, kind, kind_len) == 0 && desc[kind_len] == ':';
    }
}  // namespace

extern "C" void linuxss_restore_runtime_from_bootstrap(
    size_t bsargc, const bsheader *bsargv[]) {
    __linuxss_bsargc = bsargc;
    __linuxss_bsargv = bsargv;

    for (size_t i = 0; i < __linuxss_bsargc; ++i) {
        BootstrapRecordView view{};
        if (!bootstrap_make_view(__linuxss_bsargv[i], view)) {
            continue;
        }

        if (view.header->type == boot::TYPE_CAPEXP) {
            BootstrapCapExplainView cap_view{};
            if (!bootstrap_parse_cap_explain(view, cap_view)) {
                continue;
            }

            if (cap_view.cap_type == PayloadType::MEMORY &&
                has_memory_kind(cap_view.cap_desc, "ss-heap"))
            {
                __linuxss_ssheap_mem_cap = cap_view.cap_idx;
                continue;
            }
        }

        if (view.header->type == boot::TYPE_VADDREXP) {
            BootstrapVaddrExplainView vaddr_view{};
            if (!bootstrap_parse_vaddr_explain(view, vaddr_view)) {
                continue;
            }
            if (strcmp(vaddr_view.vaddr_desc, "#ss-heap") == 0) {
                __linuxss_ssheap_base = vaddr_view.vaddr.arith();
                __linuxss_ss_brk      = vaddr_view.vaddr.arith();
                continue;
            }
        }
    }
}
