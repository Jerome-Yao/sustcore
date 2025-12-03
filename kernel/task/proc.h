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
// 进程链表操作宏
#define PROC_LIST proc_list_head, proc_list_tail, next, prev

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
// 就绪队列操作宏
#define RP_LIST(level) rp_list_heads[level], rp_list_tails[level], snext, sprev
// 我们希望rp3队列中的进程按照run_time从小到大排序
#define RP3_LIST \
    rp_list_heads[3], rp_list_tails[3], snext, sprev, run_time, ascending

/**
 * @brief 初始化进程池
 */
void proc_init(void);

/**
 * @brief 清理PCB相关资源
 *
 * @param p PCB指针
 */
void terminate_pcb(PCB *p);

/**
 * @brief 进程测试
 *
 */
void proc_test(void);

__attribute__((section(".ptest1"))) void worker(void);
