#include <kmod/syscall.h>
#include <prm.h>

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

void *operator new(size_t size) {
    return malloc(size);
}

void operator delete(void *ptr) noexcept {
    free(ptr);
}

void *operator new[](size_t size) {
    return malloc(size);
}

void operator delete[](void *ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr, size_t) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, size_t) noexcept {
    free(ptr);
}

namespace {
    int panic_write_chunk(const char *data, size_t len, void *) {
        sys_write_serial(0, data, len);
        return len;
    }
}  // namespace

extern "C" {
void panic(const char *format, ...) {
    char chunk[256];
    va_list args;
    va_start(args, format);
    vcbprintf(chunk, sizeof(chunk), panic_write_chunk, nullptr, format, args);
    va_end(args);
    kputs("\n");
    while (true) {
    }
}
}
