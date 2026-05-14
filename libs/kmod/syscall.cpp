/**
 * @file syscall.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief syscall function
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kmod/syscall.h>
#include <prm.h>
#include <cstring>

extern "C" {
size_t brk(size_t newbrk) {
    if (newbrk == 0) {
        return __current_brk;
    }

    size_t actual_brk = sys_grow_vma(__heap_base, newbrk);
    __current_brk       = actual_brk;
    return __current_brk;
}

void *sbrk(ptrdiff_t increment) {
    size_t old_brk = __current_brk;
    size_t newbrk  = old_brk;

    if (increment >= 0) {
        size_t inc = static_cast<size_t>(increment);
        if (SIZE_MAX - old_brk < inc) {
            return reinterpret_cast<void *>(-1);
        }
        newbrk = old_brk + inc;
    } else {
        size_t dec = size_t(0) - static_cast<size_t>(increment);
        if (old_brk < dec) {
            return reinterpret_cast<void *>(-1);
        }
        newbrk = old_brk - dec;
    }

    size_t actual_brk = brk(newbrk);
    if (actual_brk != newbrk) {
        return reinterpret_cast<void *>(-1);
    }
    return reinterpret_cast<void *>(old_brk);
}
}