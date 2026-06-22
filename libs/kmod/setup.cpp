/**
 * @file setup.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief setup function
 * @version alpha-1.0.0
 * @date 2026-02-23
 *
 * @copyright Copyright (c) 2026
 *
 */

// cpp setup入口点

#include <prm.h>
#include <sustcore/capability.h>
#include <kmod/syscall.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <elf.h>

typedef void (*_init_func)(void);

extern _init_func __init_array_start[0], __init_array_end[0];

size_t __heap_base;
size_t __current_brk;
CapIdx __pcb_cap;
CapIdx __main_tcb_cap;
CapIdx __heap_mem_cap;
CapIdx __stack_mem_cap;
size_t __argc;
const char **__argv;
const char **__envp;
size_t __bsargc;
const bsheader **__bsargv;

namespace kmod {
    void init(void) {
        const size_t count1 = __init_array_end - __init_array_start;

        // 执行init
        for (size_t i = 0; i < count1; i++) {
            __init_array_start[i]();
        }
    }
}  // namespace kmod

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]);

namespace {
    bool parse_tagged_index(const char *text, const char *prefix,
                            size_t &value) {
        if (text == nullptr || prefix == nullptr) {
            return false;
        }
        size_t prefix_len = strlen(prefix);
        if (strncmp(text, prefix, prefix_len) != 0) {
            return false;
        }
        value = 0;
        for (const char *p = text + prefix_len; *p != '\0'; ++p) {
            if (*p < '0' || *p > '9') {
                return false;
            }
            value = value * 10 + static_cast<size_t>(*p - '0');
        }
        return true;
    }

    bool has_memory_kind(const char *desc, const char *kind) {
        if (desc == nullptr || kind == nullptr) {
            return false;
        }
        if (desc[0] != '#') {
            return false;
        }
        ++desc;
        size_t kind_len = strlen(kind);
        if (strncmp(desc, kind, kind_len) != 0) {
            return false;
        }
        if (desc[kind_len] != ':') {
            return false;
        }
        return desc == kind || desc[-1] == '#';
    }

    void restore_runtime_from_bootstrap() {
        for (size_t i = 0; i < __bsargc; ++i) {
            BootstrapRecordView view{};
            if (!bootstrap_make_view(__bsargv[i], view)) {
                continue;
            }

            if (view.header->type == boot::TYPE_CAPEXP) {
                BootstrapCapExplainView cap_view{};
                if (!bootstrap_parse_cap_explain(view, cap_view)) {
                    continue;
                }

                size_t tagged_idx = 0;
                if (cap_view.cap_type == PayloadType::PCB &&
                    parse_tagged_index(cap_view.cap_desc, "#self:", tagged_idx))
                {
                    __pcb_cap = cap_view.cap_idx;
                    continue;
                }
                if (cap_view.cap_type == PayloadType::TCB &&
                    parse_tagged_index(cap_view.cap_desc, "#main:", tagged_idx))
                {
                    __main_tcb_cap = cap_view.cap_idx;
                    continue;
                }
                if (cap_view.cap_type == PayloadType::MEMORY &&
                    has_memory_kind(cap_view.cap_desc, "heap"))
                {
                    __heap_mem_cap = cap_view.cap_idx;
                    continue;
                }
                if (cap_view.cap_type == PayloadType::MEMORY &&
                    has_memory_kind(cap_view.cap_desc, "stack"))
                {
                    __stack_mem_cap = cap_view.cap_idx;
                    continue;
                }
            }

            if (view.header->type == boot::TYPE_VADDREXP) {
                BootstrapVaddrExplainView vaddr_view{};
                if (!bootstrap_parse_vaddr_explain(view, vaddr_view)) {
                    continue;
                }
                if (strcmp(vaddr_view.vaddr_desc, "#heap") == 0) {
                    __heap_base   = vaddr_view.vaddr.arith();
                    __current_brk = vaddr_view.vaddr.arith();
                }
            }
        }
    }
}  // namespace

extern "C" void _cpp_setup(const void *stack_start) {
    if (stack_start == nullptr) {
        while (true) {}
    }

    auto *words = static_cast<const uint64_t *>(stack_start);
    __argc      = static_cast<size_t>(words[0]);
    __argv      = reinterpret_cast<const char **>(
        const_cast<uint64_t *>(words + 1));

    size_t offset = 1 + __argc + 1;
    __envp        = reinterpret_cast<const char **>(
        const_cast<uint64_t *>(words + offset));
    while (words[offset] != 0) {
        ++offset;
    }
    ++offset;
    while (true) {
        uint64_t type = words[offset];
        offset += 2;
        if (type == AT_NULL) {
            break;
        }
    }

    __bsargc = static_cast<size_t>(words[offset]);
    ++offset;
    __bsargv = reinterpret_cast<const bsheader **>(
        const_cast<uint64_t *>(words + offset));

    restore_runtime_from_bootstrap();

    kmod::init();

    kmod_main(static_cast<int>(__argc), __argv, __envp, __bsargv);
    while (true);
}
