/**
 * @file tcb_cap.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief TCB能力相关操作
 * @version alpha-1.0.0
 * @date 2025-12-11
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <cap/capability.h>
#include <cap/tcb_cap.h>
#include <mem/alloc.h>
#include <string.h>

#ifdef DLOG_CAP
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

//============================================
// TCB能力构造相关操作
//============================================

CapIdx create_tcb_cap(PCB *p, TCB *tcb) {
    return create_cap(p, CAP_TYPE_TCB, (void *)tcb, CAP_PRIV_ALL, nullptr);
}

CapIdx tcb_cap_derive(PCB *sproc, CapIdx sidx, PCB *dproc, qword priv) {
    TCB_CAP_START(sproc, sidx, cap, tcb, CAP_PRIV_NONE, INVALID_CAP_IDX);
    (void)tcb;

    // 进行派生
    return derive_cap(dproc, cap, priv, nullptr);
}

CapIdx tcb_cap_derive_at(PCB *sproc, CapIdx sidx, PCB *dproc, CapIdx didx,
                         qword priv) {
    TCB_CAP_START(sproc, sidx, cap, tcb, CAP_PRIV_NONE, INVALID_CAP_IDX);
    (void)tcb;

    // 进行派生
    return derive_cap_at(dproc, cap, priv, nullptr, didx);
}

CapIdx tcb_cap_clone(PCB *sproc, CapIdx sidx, PCB *dproc) {
    TCB_CAP_START(sproc, sidx, cap, tcb, CAP_PRIV_NONE, INVALID_CAP_IDX);

    (void)tcb;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return tcb_cap_derive(sproc, sidx, dproc, cap->cap_priv);
}

CapIdx tcb_cap_clone_at(PCB *sproc, CapIdx sidx, PCB *dproc,
                        CapIdx didx) {
    TCB_CAP_START(sproc, sidx, cap, tcb, CAP_PRIV_NONE, INVALID_CAP_IDX);

    (void)tcb;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return tcb_cap_derive_at(sproc, sidx, dproc, didx, cap->cap_priv);
}

CapIdx tcb_cap_degrade(PCB *p, CapIdx idx, qword cap_priv) {
    TCB_CAP_START(p, idx, cap, tcb, CAP_PRIV_NONE, INVALID_CAP_IDX);
    (void)tcb;  // 未使用, 特地标记以避免编译器警告

    if (!degrade_cap(p, cap, cap_priv)) {
        return INVALID_CAP_IDX;
    }

    return idx;
}

TCB *tcb_cap_unpack(PCB *p, CapIdx idx) {
    TCB_CAP_START(p, idx, cap, tcb, CAP_PRIV_UNPACK, nullptr);

    return tcb;
}

//============================================
// TCB能力对象操作相关操作
//============================================

/**
 * @brief 将线程切换到yield状态
 *
 * @param p 当前进程的PCB
 * @param idx 能力索引
 */
void tcb_cap_yield(PCB *p, CapIdx idx) {
    TCB_CAP_START(p, idx, cap, tcb, TCB_PRIV_YIELD, );

    // 切换线程状态到yield
    if (tcb->state != TS_RUNNING) {
        log_error("只能对运行中的线程进行yield操作! (tid=%d, state=%d)",
                  tcb->tid, tcb->state);
        return;
    }

    tcb->state = TS_YIELD;
}