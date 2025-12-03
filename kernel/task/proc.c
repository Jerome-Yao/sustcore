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

void terminate_pcb(PCB *p) {
    if (p == nullptr) {
        log_error("kill_pcb: 传入的PCB指针为空");
        return;
    }
    if (p->state != PS_ZOMBIE) {
        log_error(
            "terminate_pcb: 只能清理处于ZOMBIE状态的进程 (pid=%d, state=%d)",
            p->pid, p->state);
        return;
    }
    // 对p进行资源清理

    // 清空内核栈
    if (p->kstack != nullptr) {
        kfree(p->kstack);
        p->kstack = nullptr;
    }

    // 从进程链表中移除
    list_remove(p, PROC_LIST);

    log_debug("terminate_pcb: 进程 (pid=%d) 资源清理完成", p->pid);
}

/**
 * @brief 初始化PCB
 * 
 * @param p PCB
 * @param rp_level RP级别
 */
void init_pcb(PCB *p, int rp_level) {
    memset(p, 0, sizeof(PCB));
    p->pid      = get_current_pid();
    p->rp_level = rp_level;
    p->called_count = 0;
    p->priority = 0;
    p->run_time = 0;
    p->state    = PS_READY;
}

PCB *test_add_proc(void) {
    // 初始化PCB
    PCB *p = (PCB *)kmalloc(sizeof(PCB));
    memset(p, 0, sizeof(PCB));

    init_pcb(p, 2);

    // 设置基本信息
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
    p->ip  = (void **)&p->ctx->sepc;
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
        // 加入全局进程链表
        list_push_back(p, PROC_LIST);
    }
    cur_proc = &empty_proc;

    log_info("进程调度测试结束");
}

void worker(void) {
    // syscall(SYS_EXIT, 0);
    // asm volatile(
    //     "mv a7, %0\n"
    //     "mv a0, %1\n"
    //     "ecall\n"
    //     :
    //     : "r"(93), "r"(0)
    //     : "a0", "a7");
    // 纯净的循环，不依赖任何内核数据
    // pid > 1的进程自杀
    if (cur_proc->pid > 1) {
        cur_proc->state = PS_ZOMBIE;
        log_info("worker: 进程 (pid=%d) 状态设置为 %d", cur_proc->pid, cur_proc->state);
    }
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