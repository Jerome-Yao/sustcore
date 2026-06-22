/**
 * @file runtime.cpp
 * @author OpenAI
 * @brief linux subsystem 运行时基础支持
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <prm.h>
#include <std/assert.h>
#include <std/stdio.h>

#include <cstddef>

namespace {
    [[nodiscard]]
    void *linuxss_alloc(size_t size) {
        if (size == 0) {
            size = 1;
        }
        size_t old_brk = __linuxss_ss_brk;
        size_t new_brk = old_brk + size;
        if (new_brk < old_brk) {
            return nullptr;
        }
        size_t actual_brk = linuxss_brk(new_brk);
        if (actual_brk != new_brk) {
            return nullptr;
        }
        return reinterpret_cast<void *>(old_brk);
    }
}  // namespace

void *operator new(size_t size) {
    void *ptr = linuxss_alloc(size);
    if (ptr == nullptr) {
        panic("linux-subsystem: operator new failed size=%lu",
              static_cast<unsigned long>(size));
    }
    return ptr;
}

void operator delete(void *ptr) noexcept {
    (void)ptr;
}

void operator delete(void *ptr, size_t size) noexcept {
    (void)ptr;
    (void)size;
}

void panic(const char *format, ...) {
    printf("linux-subsystem panic: %s\n",
           format == nullptr ? "(null)" : format);
    while (true) {
    }
}
