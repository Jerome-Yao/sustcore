/**
 * @file proc.c
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "proc.h"

#include <basec/logger.h>
#include <mem/alloc.h>
#include <mem/kmem.h>
#include <mem/pmm.h>
#include <string.h>
#include <sus/boot.h>
#include <sus/ctx.h>
#include <sus/list_helper.h>
#include <sus/paging.h>
#include <sus/symbols.h>

PCB *proc_list_head;
PCB *proc_list_tail;

PCB *rp_list_heads[RP_LEVELS];
PCB *rp_list_tails[RP_LEVELS];

PCB *cur_proc;

PCB empty_proc;

#define PROC_LIST proc_list_head, proc_list_tail, next, prev
#define RP_LIST(level) rp_list_heads[level], rp_list_tails[level], snext, sprev
// 我们希望rp3队列中的进程按照run_time从小到大排序
#define RP3_LIST rp_list_heads[3], rp_list_tails[3], snext, sprev, run_time, ascending
#define APPLY_MACRO(macro, args) macro args

/**
 * @brief 初始化进程池
 *
 */
void proc_init(void) {
    // 设置链表头
    list_init(PROC_LIST);
    list_init(RP_LIST(0));
    list_init(RP_LIST(1));
    list_init(RP_LIST(2));
    // 设定rp3为有序链表
    ordered_list_init(RP3_LIST);

    // 将当前进程设为空
    cur_proc = nullptr;

    // 设置空闲进程
    // PID 0 保留给空进程
    memset(&empty_proc, 0, sizeof(PCB));
}

/**
 * @brief 获得下一个就绪进程, 同时将其弹出就绪队列
 *
 * @return PCB* 下一个就绪进程的PCB指针, 如果是nullptr, 则表示发生错误;
 * 如果是cur_proc, 则表示继续运行当前进程
 */
static PCB *fetch_ready_process(void) {
    if (cur_proc == nullptr) {
        // 这是不可能发生的情况, 报错
        log_error("fetch_ready_process: 当前没有运行的进程");
        return nullptr;
    }

    // 如果当前进程已是rp0, 且仍然处于RUNNING状态, 则继续运行该进程
    if (cur_proc->rp_level == 0 && cur_proc->state == RUNNING) {
        return cur_proc;
    }

    // 否则当前进程要么不是rp0, 要么不是RUNNING状态
    // 那么我们就可以从rp0开始寻找下一个就绪进程
    PCB *next;
    list_pop_front(next, RP_LIST(0));
    if (next != nullptr) {
        return next;
    }

    // 寻找rp1
    // 如果当前进程已是rp1, 且仍然处于RUNNING状态, 且时间片未用尽,
    // 则继续运行该进程
    if (cur_proc->rp_level == 1 && cur_proc->state == RUNNING &&
        cur_proc->rp1_count > 0)
    {
        return cur_proc;
    }

    // 否则从rp1队列中弹出下一个就绪进程
    list_pop_front(next, RP_LIST(1));
    if (next != nullptr) {
        return next;
    }

    // 寻找rp2
    // 如果当前进程已是rp2, 且仍然处于RUNNING状态, 且时间片未用尽,
    // 则继续运行该进程
    if (cur_proc->rp_level == 2 && cur_proc->state == RUNNING &&
        cur_proc->rp2_count > 0)
    {
        return cur_proc;
    }
    list_pop_front(next, RP_LIST(2));
    if (next != nullptr) {
        return next;
    }

    // 寻找rp3
    // 如果当前进程已是rp3, 且仍然处于RUNNING状态, 则继续运行该进程
    if (cur_proc->rp_level == 3 && cur_proc->state == RUNNING) {
        return cur_proc;
    }
    // 取出有序链表中run_time最小的进程(位于头部)
    ordered_list_pop_front(next, RP3_LIST);
    if (next != nullptr) {
        return next;
    }

    // 均不符合要求
    return nullptr;
}

/**
 * @brief 调度器 - 选择下一个就绪进程进行切换
 *
 */
void schedule(RegCtx **old) {
    // cur_proc 为空, 还未到进程调度阶段
    if (cur_proc == nullptr) {
        log_error("schedule: 当前没有运行的进程");
        return;
    }

    log_debug("schedule: 当前进程 pid=%d", cur_proc ? cur_proc->pid : -1);
    log_debug("current state: %d", cur_proc ? cur_proc->state : -1);

    // 更新当前进程的上下文
    cur_proc->ctx = *old;
    // 如果是rp1/rp2进程, 则更新其时间片计数器
    if (cur_proc->rp_level == 1) {
        cur_proc->rp1_count--;
    } else if (cur_proc->rp_level == 2) {
        cur_proc->rp2_count--;
    }
    // 如果是rp3进程, 则更新其运行时间统计
    else if (cur_proc->rp_level == 3) {
        cur_proc->run_time += 10;  // TODO: 根据已经过去的周期数进行更新
    }

    // 获取下一个就绪进程
    PCB *next = fetch_ready_process();

    // 继续运行当前进程
    if (next == cur_proc) {
        log_debug("继续运行当前进程 (pid=%d), rp_level = %d", cur_proc->pid, cur_proc->rp_level);
        if (cur_proc->rp_level == 1) {
            log_debug("RP1: 剩余时间片为%d", cur_proc->rp1_count);
        } else if (cur_proc->rp_level == 2) {
            log_debug("RP2: 剩余时间片为%d", cur_proc->rp2_count);
        }else if (cur_proc->rp_level == 2) {
            log_debug("RP3: 已运行%d ms", cur_proc->run_time);
        }
        return;
    }

    // 没找到任何可运行的进程
    if (next == nullptr) {
        // 如果当前进程仍然是RUNNING状态, 则继续运行当前进程
        if (cur_proc->state == RUNNING) {
            next = cur_proc;
        }
        else {
            log_debug("所有进程均运行");
            // 目前没有任何进程可以运行.
            // 此时应当调度一个备用的空进程
            // 但是我们还没做XD
            log_error("schedule: 没有可运行的进程, 系统停滞");
            while (true);
        }
    }

    // 否则, 切换到下一个进程
    *old = next->ctx;
    // TODO: 更新页表与其它控制寄存器

    // 更新进程状态
    // 如果当前进程仍然是RUNNING状态, 则将其设为READY
    int prev_pid = cur_proc->pid;
    if (cur_proc->state == RUNNING) {
        cur_proc->state = READY;
    }
    // 加入到相应的就绪队列
    if (cur_proc->state == READY) {
        if (cur_proc->rp_level == 3) {
            // rp3为有序链表
            ordered_list_insert(cur_proc, RP3_LIST);
        } else {
            // 其它rp队列为普通链表, 加到尾部(先到先得)
            list_push_back(cur_proc, RP_LIST(cur_proc->rp_level));
        }
    }

    // 将当前进程更新到下一个进程
    cur_proc  = next;
    cur_proc->state = RUNNING;

    // 如果是rp1/rp2进程, 则重置其时间片计数器
    if (cur_proc->rp_level == 1) {
        cur_proc->rp1_count = 5;  // TODO: 时间片长度待定
    } else if (cur_proc->rp_level == 2) {
        cur_proc->rp2_count = 3;  // TODO: 时间片长度待定
    }

    log_debug("调度: 从进程 (pid=%d) 切换到进程 (pid=%d)", prev_pid, cur_proc->pid);
    // 根据rp_level打印调度信息
    if (cur_proc->rp_level == 0) {
        log_debug("调度到 rp0 实时进程 (pid=%d)", cur_proc->pid);
    } else if (cur_proc->rp_level == 1) {
        log_debug("调度到 rp1 服务进程 (pid=%d)", cur_proc->pid);
    } else if (cur_proc->rp_level == 2) {
        log_debug("调度到 rp2 普通用户进程 (pid=%d)", cur_proc->pid);
    } else if (cur_proc->rp_level == 3) {
        log_debug("调度到 rp3 Daemon进程 (pid=%d)", cur_proc->pid);
    }
}

PCB *test_add_proc(void) {
    // 初始化PCB
    PCB *p = (PCB *)kmalloc(sizeof(PCB));
    memset(p, 0, sizeof(PCB));

    // 设置基本信息
    p->pid   = get_current_pid();
    p->rp_level = 2;  // 普通用户进程
    p->priority = 0;
    p->state = READY;

    // 分配内核栈
    p->kstack = (umb_t *)kmalloc(PAGE_SIZE);
    log_info("为进程(PID:%d)分配内核栈: %p", p->pid, p->kstack);

    log_info("为进程(PID:%d)初始化上下文", p->pid);
    // 栈顶(栈是向下增长的)
    umb_t *stack_top  = (umb_t *)((umb_t)p->kstack + PAGE_SIZE);
    // 留出空间存放上下文
    stack_top        -= sizeof(RegCtx) / sizeof(umb_t);
    p->ctx            = (RegCtx *)stack_top;
    memset(p->ctx, 0, sizeof(RegCtx));
    // 设置进程入口点与ip寄存器
    // 由于只是测试例程, 所以arch有关也是没事的
    p->ip = (void **)&p->ctx->sepc;
    *p->ip = (void *)worker;

    // 1. 分配一页作为用户栈 (alloc_page 返回物理地址)
    umb_t user_stack_pa = (umb_t)alloc_page();

    // 2. 获取内核虚拟地址以便清空栈内容
    void *user_stack_kva = PA2KPA(user_stack_pa);
    memset(user_stack_kva, 0, PAGE_SIZE);

    // 3. 栈向下增长，所以 SP 指向页面末尾
    p->ctx->regs[1] = user_stack_pa + PAGE_SIZE;

    log_info("进程(PID=%d)用户栈: PA=0x%lx, SP=0x%lx", p->pid, user_stack_pa,
                p->ctx->regs[1]);

    // ra
    p->ctx->regs[0]      = 0;
    // 代码运行在 S-Mode
    p->ctx->sstatus.spp  = 1;
    // 用户进程应该开启中断
    p->ctx->sstatus.spie = 1;

    return p;
}

/**
 * @brief 进程调度测试
 * 
 */
void proc_test(void) {
    log_info("进程调度测试开始");

    // 添加3个测试进程
    for (int i = 0; i < 3; i++) {
        PCB *p = test_add_proc();
        log_info("添加测试进程 PID=%d", p->pid);
        // 将其加入就绪队列
        list_push_back(p, RP_LIST(p->rp_level));
    }
    cur_proc = &empty_proc;

    log_info("进程调度测试结束");
}

// RegCtx *exit_current_task() {
//     if (cur_proc == nullptr) {
//         log_error("exit_current_task: 当前没有运行的进程");
//         return nullptr;
//     }
//     cur_proc->state = ZOMBIE;

//     RegCtx *next_ctx = schedule(cur_proc->ctx);

//     return next_ctx;
// }

__attribute__((section(".ptest1"), used)) void worker(void) {
    // syscall(SYS_EXIT, 0);
    // asm volatile(
    //     "mv a7, %0\n"
    //     "mv a0, %1\n"
    //     "ecall\n"
    //     :
    //     : "r"(93), "r"(0)
    //     : "a0", "a7");
    // 纯净的循环，不依赖任何内核数据
    while (1) {
        // 简单的忙等待，防止被编译器优化掉
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void cal_prime(void) {
    int n = 1000000;
    for (size_t j = 1; j < n; j++) {
        if (j <= 1 || ((j > 2) && (j % 2 == 0)))
            continue;

        int cnt = 0;

        if (j == 2) {
            kprintf("PRIME: %d is prime\n", j);
            continue;
        } else {
            for (int i = 3; i * i <= j; i += 2) {
                if (j % i == 0)
                    cnt++;
            }
            if (cnt > 0)
                continue;
            // else n is prime
            else
                kprintf("PRIME: %d is prime\n", j);
        }
    }
}