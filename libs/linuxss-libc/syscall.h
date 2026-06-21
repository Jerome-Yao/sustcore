/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief linux subsystem libc syscall declarations
 * @version alpha-1.0.0
 * @date 2026-06-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <sustcore/capability.h>

struct VMAInfo {
    b64 vma_type;
    b64 vma_prot;
    void *vma_start;
    size_t vma_size;
    CapIdx mem_cap;
};

extern "C" void sys_write_serial(const char *str, size_t len);
extern "C" CapIdx sys_mem_create(size_t memsz, bool shared, bool continuity,
                                 uint64_t growth);
extern "C" bool sys_mem_resize(CapIdx idx, size_t newsz);
extern "C" bool sys_pcb_map(CapIdx pcb_cap, CapIdx mem_cap, size_t offset,
                            void *vaddr, size_t sz, uint64_t protflg);
extern "C" bool sys_pcb_unmap(CapIdx pcb_cap, void *vaddr, size_t sz);
extern "C" size_t sys_pcb_query_vspace(CapIdx pcb_cap, size_t offset,
                                       VMAInfo *info_array,
                                       size_t max_entries);
