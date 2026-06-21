/**
 * @file syscall.cpp
 * @author OpenAI
 * @brief linux subsystem libc syscall helpers
 * @version alpha-1.0.0
 * @date 2026-06-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <prm.h>
#include <syscall.h>

extern "C" size_t linux_brk(size_t newbrk) {
    if (newbrk == 0) {
        return __linuxss_brk;
    }

    if (newbrk < __linuxss_heap_base) {
        return __linuxss_brk;
    }
    if (__linuxss_heap_mem_cap == cap::null) {
        return __linuxss_brk;
    }
    if (!sys_mem_resize(__linuxss_heap_mem_cap,
                        newbrk - __linuxss_heap_base))
    {
        return __linuxss_brk;
    }

    __linuxss_brk = newbrk;
    return __linuxss_brk;
}

extern "C" size_t linuxss_brk(size_t newbrk) {
    if (newbrk == 0) {
        return __linuxss_ss_brk;
    }

    if (newbrk < __linuxss_ssheap_base) {
        return __linuxss_ss_brk;
    }
    if (__linuxss_ssheap_mem_cap == cap::null) {
        return __linuxss_ss_brk;
    }
    if (!sys_mem_resize(__linuxss_ssheap_mem_cap,
                        newbrk - __linuxss_ssheap_base))
    {
        return __linuxss_ss_brk;
    }

    __linuxss_ss_brk = newbrk;
    return __linuxss_ss_brk;
}
