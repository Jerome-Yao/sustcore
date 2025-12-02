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

#include <sus/bits.h>
#include <task/task_struct.h>

/**
 * @brief 进程链表
 * 
 */
extern PCB *proc_list_head;
extern PCB *proc_list_tail;

/**
 * @brief 空进程
 * 
 */
extern PCB empty_proc;

// 进程就绪队列级别
#define RP_LEVELS (4)

/**
 * @brief 当前进程
 * 
 */
extern PCB *cur_proc;

// 就绪队列链表
extern PCB *rp_list_heads[RP_LEVELS];
extern PCB *rp_list_tails[RP_LEVELS];

/**
 * @brief 初始化进程池
 */
void proc_init(void);

/**
 * @brief 调度器 - 从当前进程切换到下一个就绪进程
 */
void schedule(RegCtx **ctx);

void proc_test(void);

RegCtx *exit_current_task();

__attribute__((section(".ptest1"))) void worker(void);
