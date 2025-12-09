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

#include <cap/pcb_cap.h>
#include <mem/alloc.h>
#include <mem/kmem.h>
#include <mem/pmm.h>
#include <mem/vmm.h>
#include <string.h>
#include <sus/boot.h>
#include <sus/ctx.h>
#include <sus/list_helper.h>
#include <sus/paging.h>
#include <sus/symbols.h>

#ifdef DLOG_TASK
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

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

void init_pcb(PCB *p, int rp_level) {
    memset(p, 0, sizeof(PCB));
    p->pid          = get_current_pid();
    p->rp_level     = rp_level;
    p->called_count = 0;
    p->priority     = 0;
    p->run_time     = 0;
    p->state        = PS_READY;

    // 加入到全局进程链表
    list_push_back(p, PROC_LIST);

    // 构造cspaces
    p->cap_spaces = (CSpace *)kmalloc(sizeof(CSpace) * PROC_CSPACES);
    memset(p->cap_spaces, 0, sizeof(CSpace) * PROC_CSPACES);
}

PCB *create_pcb(TM *tm, void *entrypoint, int rp_level, PCB *parent) {
    if (rp_level < 0 || rp_level >= RP_LEVELS) {
        log_error("new_task: 无效的RP级别 %d", rp_level);
        return nullptr;
    }

    if (entrypoint == nullptr) {
        log_error("new_task: 无效的进程入口点");
        return nullptr;
    }

    // 初始化PCB
    PCB *p = (PCB *)kmalloc(sizeof(PCB));
    init_pcb(p, rp_level);

    // 设置基本信息
    p->tm = tm;

    // 入口点
    p->entrypoint = entrypoint;

    // 设置父子关系
    p->parent = parent;
    if (parent != nullptr) {
        list_push_back(p, CHILDREN_TASK_LIST(parent));
    }

    // 分配内核栈
    log_info("为进程(PID:%d)分配内核栈: %p", p->pid, p->kstack);
    p->kstack = (umb_t *)kmalloc(PAGE_SIZE);
    memset(p->kstack, 0, PAGE_SIZE);

    log_info("为进程(PID:%d)初始化上下文", p->pid);
    // 留出空间存放上下文
    umb_t *stack_top  = (umb_t *)((umb_t)p->kstack + PAGE_SIZE);
    stack_top        -= sizeof(RegCtx) / sizeof(umb_t);
    p->ctx            = (RegCtx *)stack_top;
    memset(p->ctx, 0, sizeof(RegCtx));

    // 架构相关设置
    arch_setup_proc(p);
    return p;
}

PCB *new_task(TM *tm, void *stack, void *heap, void *entrypoint, int rp_level, PCB *parent)
{
    if (stack == nullptr) {
        log_error("new_task: 无效的栈地址");
        return nullptr;
    }

    if (heap == nullptr) {
        log_error("new_task: 无效的堆地址");
        return nullptr;
    }

    // 构造pcb
    PCB *p = create_pcb(tm, entrypoint, rp_level, parent);
    if (p == nullptr) {
        log_error("new_task: 无法创建PCB");
        return nullptr;
    }

    // 初始化进程
    // 设置64KB的初始栈(stack为栈顶)
    void *stack_end = stack - 16 * PAGE_SIZE;
    add_vma(p->tm, stack_end, 16 * PAGE_SIZE, VMAT_STACK);
    // 设置128MB的堆
    add_vma(p->tm, heap, 32768 * PAGE_SIZE, VMAT_HEAP);

    // 预分配4KB
    alloc_pages_for(p->tm, stack_end + 15 * PAGE_SIZE, 1, RWX_MODE_RW, true);
    alloc_pages_for(p->tm, heap, 1, RWX_MODE_RW, true);

    // ip, sp寄存器
    *p->ip = entrypoint;
    *p->sp = (void *)((umb_t)stack);

    // 为当前进程构造自己的PCB能力
    CapPtr pcb_cap_ptr = create_pcb_cap(p, p,
                                        (PCBCapPriv){
                                            .priv_yield = true,
                                            .priv_exit  = true,
                                            .priv_fork  = true,
                                            .priv_getpid = true,
                                        });
    // 将PCB能力传递给进程作为第一个参数
    arch_setup_argument(p, 0, pcb_cap_ptr.val);

    // 将堆指针传递给进程作为第二个参数
    arch_setup_argument(p, 1, (umb_t)(heap));

    // 加入到就绪队列
    if (rp_level != 3)
        list_push_back(p, RP_LIST(rp_level));
    else
        ordered_list_insert(p, RP3_LIST);

    return p;
}

PCB *fork_task(PCB *parent) {
    // 首先对内存进行操作
    TM *new_tm = setup_task_memory();
    // 逐VMA复制
    VMA *vma;
    foreach_ordered_list(vma, TM_VMA_LIST(parent->tm)) {
        clone_vma(parent->tm, vma, new_tm);
    }

    // 构造新的PCB
    PCB *p = create_pcb(new_tm, parent->entrypoint, parent->rp_level, parent);

    if (p == nullptr) {
        log_error("fork_task: 无法创建子进程");
        return nullptr;
    }

    // 对fork进程进行初始化
    // 栈, 堆无需额外处理, 因为已经在clone_vma中完成
    // 复制上下文
    memcpy(p->ctx, parent->ctx, sizeof(RegCtx));

    // 对Capability的复制等交由调用者完成
    // 加入到就绪队列
    if (p->rp_level != 3)
        list_push_back(p, RP_LIST(p->rp_level));
    else
        ordered_list_insert(p, RP3_LIST);

    mem_display_mapping_layout(p->tm->pgd);
    return p;
}