/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/trait.h>
#include <basecpp/baseio.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/pfa.h>
#include <sus/types.h>

#include <cstdarg>
#include <cstring>
#include "mem/kaddr.h"
#include <symbols.h>

bool post_init_flag = false;

void post_init(void) {}

void init(void) {}

int kputs(const char* str) {
    ArchSerial::serial_write_string(str);
    return strlen(str);
}

int kputchar(char ch) {
    ArchSerial::serial_write_char(ch);
    return ch;
}

char kgetchar() {
    return '\0';
}

KernelIO kio;

int KernelIO::putchar(char c) {
    return kputchar(c);
}

int KernelIO::puts(const char* str) {
    return kputs(str);
}

char KernelIO::getchar() {
    return kgetchar();
}

int kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vbprintf(kio, fmt, args);
    va_end(args);
    return len;
}

void kernel_setup(void) {
    ArchSerial::serial_write_string("欢迎使用 Sustcore Riscv64 内核!\n");
    ArchInitialization::pre_init();

    MemRegion regions[128];
    int cnt = ArchMemoryLayout::detect_memory_layout(regions, 128);
    for (int i = 0; i < cnt; i++) {
        kprintf("Region %d: [%p, %p) Status: %d\n", i, regions[i].ptr,
                (void*)((umb_t)(regions[i].ptr) + regions[i].size),
                static_cast<int>(regions[i].status));
    }

    kprintf("初始化线性增长PFA\n");

    LinearGrowPFA::pre_init(regions, cnt);

    kprintf("=======页表管理器测试========\n");

    char *test_addr1 = (char *)0x80900000;
    char *test_addr2 = (char *)0x80A00000;
    *test_addr1    = 'A';
    *test_addr2    = 'B';
    // 测试映射结果
    kprintf("==============\n");
    kprintf("*%p: %c, *%p: %c \n", test_addr1, *test_addr1, test_addr2, *test_addr2);
    kprintf("==============\n");

    // 页表管理器测试
    typedef Riscv64SV39PageMan<LinearGrowPFA> TestPageMan;
    TestPageMan pageman;
    // 保护内核应该会映射的区域
    kprintf("[0x80000000 ~ 0x90000000) -> [0x80000000 ~ 0x90000000) \n");
    pageman.map_range<true>((void *)0x80000000, (void *)0x80000000, 0x800000,
                            TestPageMan::rwx(true, true, true),
                            false, false);
    // 映射测试地址
    pageman.map_range<true>(
        (void *)test_addr1, (void *)test_addr1, 0x100000,
        TestPageMan::rwx(true, true, false), false, false);
    pageman.map_range<true>(
        (void *)test_addr2, (void *)test_addr2, 0x100000,
        TestPageMan::rwx(true, true, false), false, false);
    pageman.switch_root();
    TestPageMan::flush_tlb();
    // 测试映射结果
    kprintf("==============\n");
    kprintf("*%p: %c, *%p: %c \n", test_addr1, *test_addr1, test_addr2, *test_addr2);
    kprintf("==============\n");

    // 映射测试地址
    pageman.map_range<true>(
        (void *)test_addr1, (void *)test_addr2, 0x100000,
        TestPageMan::rwx(true, true, false), false, false);
    pageman.map_range<true>(
        (void *)test_addr2, (void *)test_addr1, 0x100000,
        TestPageMan::rwx(true, true, false), false, false);
    pageman.switch_root();
    TestPageMan::flush_tlb();
    // 测试映射结果
    kprintf("==============\n");
    kprintf("*%p: %c, *%p: %c \n", test_addr1, *test_addr1, test_addr2, *test_addr2);
    kprintf("==============\n");

    while (true);
}