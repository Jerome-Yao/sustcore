/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <basec/logger.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

// 目前这部分代码只是调试作用

// 0x10000000 is memory-mapped address of UART according to device tree
#define UART_ADDR 0x10000000

/*
 * Initialize NS16550A UART
 */
void uart_init(size_t base_addr) {
    volatile uint8_t *ptr = (uint8_t *)base_addr;
    const uint8_t LCR = 0b11;

    ptr[3] = LCR;
    ptr[2] = 0b1;
    ptr[1] = 0b1;
}

static void uart_put(size_t base_addr, uint8_t c) {
    *(uint8_t *)base_addr = c;
}

int kputchar(int character) {
    uart_put(UART_ADDR, (uint8_t)character);
    return character;
}

int kputs(const char *str) {
    const char *base = str;
    while (*str) {
        kputchar((int)*str);
        ++str;
    }
    return str - base + 1;
}

/**
 * @brief 内核主函数
 * 
 * @return int 
 */
int main(void) {
    log_info("Hello RISCV World!");
    return 0;
}

/**
 * @brief 初始化
 * 
 */
void init(void) {
    uart_init(UART_ADDR);

    kputs("\n");
    init_logger(kputs, "SUSTCore");
}

/**
 * @brief 收尾工作
 * 
 */
void terminate(void) {
    while(true);
}

/**
 * @brief 内核启动函数
 *
 */
void c_setup(void) {
    init();

    main();

    terminate();
}