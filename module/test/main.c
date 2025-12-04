/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试模块主文件
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <sus/bits.h>
#include <sus/syscall.h>
#include <basec/baseio.h>
#include <string.h>

void __yield__() {
    asm volatile(
        "mv a7, %0\n"
        "ecall\n"
        :
        : "r"(SYS_YIELD)
        : "a0", "a7");
}

void __log__(const char *msg) {
    asm volatile(
        "mv a7, %0\n"
        "mv a0, %1\n"
        "ecall\n"
        :
        : "r"(SYS_LOG), "r"(msg)
        : "a0", "a7");
}

bool test_prime(int k) {
    if (k <= 1) return false;
    for (int i = 2 ; i * i <= k ; i ++) {
        if (k % i == 0) return false;
    }
    return true;
}

bool test_square(int k) {
    int root = 0;
    while (root * root < k) {
        root++;
    }
    return (root * root == k);
}

void test1(void) {
    char buffer[256];
    for (int i = 3000'0000 ; i < 4000'0000 ; i ++) {
        if (test_square(i)) {
            int len = ssprintf(buffer, "Square: %d", i);
            buffer[len] = '\0';
            __log__(buffer);
        }
    }
}

void test2(void) {
    char buffer[256];
    for (int i = 4000'0000 ; i < 5000'0000 ; i ++) {
        if (test_square(i)) {
            int len = ssprintf(buffer, "Square: %d", i);
            buffer[len] = '\0';
            __log__(buffer);
        }
    }
}

void test3(void) {
    char buffer[256];
    for (int i = 5000'0000 ; i < 6000'0000 ; i ++) {
        if (test_square(i)) {
            int len = ssprintf(buffer, "Square: %d", i);
            buffer[len] = '\0';
            __log__(buffer);
        }
    }
}

void test4(void) {
    char buffer[256];
    for (int i = 6000'0000 ; i < 7000'0000 ; i ++) {
        if (test_square(i)) {
            int len = ssprintf(buffer, "Square: %d", i);
            buffer[len] = '\0';
            __log__(buffer);
        }
    }
}

extern umb_t arg[8];

int kmod_main(void) {
    umb_t pid = arg[1];
    char buffer[256];
    ssprintf(buffer, "测试模块启动! PID=%d", pid);
    __log__(buffer);
    switch (pid)
    {
    case 1:
        test1();
        break;
    case 2:
        test2();
        break;
    case 3:
        test3();
        break;
    default:
        test4();
        break;
    }
    while (true);
    return 0;
}