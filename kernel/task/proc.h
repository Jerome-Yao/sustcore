/**
 * @file proc.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-24
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <arch/riscv64/int/trap.h>
#include <sus/bits.h>

#define POOL_SIZE 2

typedef InterruptContextRegisterList RegCtx;

// Saved registers for kernel context switches.
// typedef struct Context {
//     umb_t ra;
//     umb_t sp;

//     // callee-saved
//     umb_t s0;
//     umb_t s1;
//     umb_t s2;
//     umb_t s3;
//     umb_t s4;
//     umb_t s5;
//     umb_t s6;
//     umb_t s7;
//     umb_t s8;
//     umb_t s9;
//     umb_t s10;
//     umb_t s11;
// } Context;

typedef enum {
    READY   = 0,
    RUNNING = 1,
    BLOCKED = 2,
    ZOMBIE  = 3,
} ProcState;

typedef struct ProcessControlBlock {
    umb_t pid;
    ProcState state;
    RegCtx *ctx;
    umb_t *kstack;  // 内核栈
    // TODO

} ProcessControlBlock;

/**
 * @brief 实现 context switch
 *
 * @param old  旧进程 context 结构体指针
 * @param new  新进程 context 结构体指针
 */
void __switch(RegCtx *old, RegCtx *new);

/**
 * @brief 初始化进程管理系统
 */
void proc_init(void);

/**
 * @brief 调度器 - 从当前进程切换到下一个就绪进程
 */
RegCtx *schedule(RegCtx *old);

void worker(void);

void cal_prime(void);