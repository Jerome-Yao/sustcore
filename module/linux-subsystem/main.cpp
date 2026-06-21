/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief POSIX subsystem main file
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <elf.h>
#include <std/stdio.h>
#include <sus/types.h>
#include <sustcore/bootstrap.h>
#include <sustcore/syscall_str.h>
#include <syscall.h>

#include <cstddef>
#include <syscall.h.in>

extern "C" bool g_linux_initialized = false;

extern "C" size_t linux_dispatch(size_t a0, size_t a1, size_t a2, size_t a3,
                                 size_t a4, size_t a5, size_t a6, size_t a7);

#define INVALID_VALUE 0xFFFF'FFFF'FFFF'FFFF

const char *at_to_string(int a_type) {
    switch (a_type) {
        case AT_NULL:              return "AT_NULL";
        case AT_IGNORE:            return "AT_IGNORE";
        case AT_EXECFD:            return "AT_EXECFD";
        case AT_PHDR:              return "AT_PHDR";
        case AT_PHENT:             return "AT_PHENT";
        case AT_PHNUM:             return "AT_PHNUM";
        case AT_PAGESZ:            return "AT_PAGESZ";
        case AT_BASE:              return "AT_BASE";
        case AT_FLAGS:             return "AT_FLAGS";
        case AT_ENTRY:             return "AT_ENTRY";
        case AT_NOTELF:            return "AT_NOTELF";
        case AT_UID:               return "AT_UID";
        case AT_EUID:              return "AT_EUID";
        case AT_GID:               return "AT_GID";
        case AT_EGID:              return "AT_EGID";
        case AT_PLATFORM:          return "AT_PLATFORM";
        case AT_HWCAP:             return "AT_HWCAP";
        case AT_CLKTCK:            return "AT_CLKTCK";
        case AT_FPUCW:             return "AT_FPUCW";
        case AT_DCACHEBSIZE:       return "AT_DCACHEBSIZE";
        case AT_ICACHEBSIZE:       return "AT_ICACHEBSIZE";
        case AT_UCACHEBSIZE:       return "AT_UCACHEBSIZE";
        case AT_IGNOREPPC:         return "AT_IGNOREPPC";
        case AT_SECURE:            return "AT_SECURE";
        case AT_BASE_PLATFORM:     return "AT_BASE_PLATFORM";
        case AT_RANDOM:            return "AT_RANDOM";
        case AT_HWCAP2:            return "AT_HWCAP2";
        case AT_RSEQ_FEATURE_SIZE: return "AT_RSEQ_FEATURE_SIZE";
        case AT_RSEQ_ALIGN:        return "AT_RSEQ_ALIGN";
        case AT_HWCAP3:            return "AT_HWCAP3";
        case AT_HWCAP4:            return "AT_HWCAP4";
        case AT_EXECFN:            return "AT_EXECFN";
        case AT_SYSINFO:           return "AT_SYSINFO";
        case AT_SYSINFO_EHDR:      return "AT_SYSINFO_EHDR";
        case AT_L1I_CACHESHAPE:    return "AT_L1I_CACHESHAPE";
        case AT_L1D_CACHESHAPE:    return "AT_L1D_CACHESHAPE";
        case AT_L2_CACHESHAPE:     return "AT_L2_CACHESHAPE";
        case AT_L3_CACHESHAPE:     return "AT_L3_CACHESHAPE";
        case AT_L1I_CACHESIZE:     return "AT_L1I_CACHESIZE";
        case AT_L1I_CACHEGEOMETRY: return "AT_L1I_CACHEGEOMETRY";
        case AT_L1D_CACHESIZE:     return "AT_L1D_CACHESIZE";
        case AT_L1D_CACHEGEOMETRY: return "AT_L1D_CACHEGEOMETRY";
        case AT_L2_CACHESIZE:      return "AT_L2_CACHESIZE";
        case AT_L2_CACHEGEOMETRY:  return "AT_L2_CACHEGEOMETRY";
        case AT_L3_CACHESIZE:      return "AT_L3_CACHESIZE";
        case AT_L3_CACHEGEOMETRY:  return "AT_L3_CACHEGEOMETRY";
        case AT_MINSIGSTKSZ:       return "AT_MINSIGSTKSZ";
        default:                   return "UNKNOWN";
    }
}

void dump_vector(const char *name, const char *const *items) {
    if (items == nullptr) {
        printf("%s: (null)\n", name);
        return;
    }
    for (size_t i = 0; items[i] != nullptr; ++i) {
        printf("%s[%u] = %s\n", name, static_cast<unsigned>(i), items[i]);
    }
}

void dump_auxv(const Elf64_auxv_t *auxv) {
    if (auxv == nullptr) {
        printf("auxv: (null)\n");
        return;
    }
    for (size_t i = 0; auxv[i].a_type != AT_NULL; ++i) {
        printf("auxv[%u] = { type=%s, val=%p }\n", static_cast<unsigned>(i),
               at_to_string(auxv[i].a_type),
               reinterpret_cast<void *>(auxv[i].a_un.a_val));
    }
    printf("auxv[end] = { type=%s, val=%p }\n", at_to_string(AT_NULL), nullptr);
}

void dump_bsargv(size_t bsargc, const bsheader *const *bsargv) {
    if (bsargv == nullptr) {
        printf("bsargv: (null)\n");
        return;
    }
    for (size_t i = 0; i < bsargc; ++i) {
        const bsheader *record = bsargv[i];
        if (record == nullptr) {
            printf("bsargv[%u] = nullptr\n", static_cast<unsigned>(i));
            continue;
        }
        printf("bsargv[%u] = { type=%u, size=%u }\n", static_cast<unsigned>(i),
               record->type, record->size);
    }
}

void stack_dump(const void *stack_sp, const Elf64_auxv_t *auxv) {
    if (auxv == nullptr) {
        printf("stack_dump: auxv=null\n");
        return;
    }
    if (stack_sp == nullptr) {
        printf("stack_dump: stack_sp=null\n");
        return;
    }

    auto *begin = static_cast<const uint64_t *>(stack_sp);
    auto *end   = reinterpret_cast<const Elf64_auxv_t *>(auxv);
    while (end->a_type != AT_NULL) {
        ++end;
    }
    ++end;

    auto *word_end = reinterpret_cast<const uint64_t *>(end);
    for (auto *line = begin; line < word_end; line += 2) {
        printf("%p:", line);
        for (size_t i = 0; i < 2 && line + i < word_end; ++i) {
            printf(" 0x%016lx", line[i]);
        }
        printf("\n");
    }
}

extern "C" void linux_main(const void *stack_sp, size_t argc,
                           const char *argv[], const char *envp[],
                           const Elf64_auxv_t *auxv, size_t bsargc,
                           const bsheader *bsargv[]) {
    g_linux_initialized = true;
    puts("linux-subsystem: initialized");
    printf("stack dump:\n");
    stack_dump(stack_sp, auxv);
    printf("argc & argv:\n");
    printf("argc = %u\n", static_cast<unsigned>(argc));
    dump_vector("argv", argv);
    printf("\nenvp:\n");
    dump_vector("envp", envp);
    printf("\nauxv:\n");
    dump_auxv(auxv);
    printf("\nbsargc & bsargv:\n");
    printf("bsargc = %u\n", static_cast<unsigned>(bsargc));
    dump_bsargv(bsargc, bsargv);
}

size_t linux_sys_write(size_t fd, const void *buf, size_t len) {
    if (fd == 1) {
        sys_write_serial(reinterpret_cast<const char *>(buf), len);
        return len;
    }
    printf("linux-subsystem: unsupported fd %d\n", fd);
    return INVALID_VALUE;
}

extern "C" size_t linux_dispatch(size_t a0, size_t a1, size_t a2, size_t a3,
                                 size_t a4, size_t a5, size_t a6, size_t a7) {
    switch (a7) {
        case __NR_write:
            return linux_sys_write(a0, reinterpret_cast<const void *>(a1), a2);
        default:
            printf("linux-subsystem: unsupported syscall %s (%d)\n",
                   syscall_to_string(a7), a7);
            // 先卡死以进行测试
            while (true);
            return INVALID_VALUE;
    }
}
