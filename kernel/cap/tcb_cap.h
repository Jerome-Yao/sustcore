/**
 * @file tcb_cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief TCB能力相关操作
 * @version alpha-1.0.0
 * @date 2025-12-11
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <cap/capability.h>
#include <task/task_struct.h>

// 设置线程优先级权限
#define TCB_PRIV_SET_PRIORITY      (0x0000'0000'0001'0000ull)
// 挂起线程权限
#define TCB_PRIV_SUSPEND           (0x0000'0000'0002'0000ull)
// 恢复线程权限
#define TCB_PRIV_RESUME            (0x0000'0000'0004'0000ull)
// 终止线程权限
#define TCB_PRIV_TERMINATE         (0x0000'0000'0008'0000ull)
// 线程yield权限
#define TCB_PRIV_YIELD             (0x0000'0000'0010'0000ull)
// 线程等待通知权限
#define TCB_PRIV_WAIT_NOTIFICATION (0x0000'0000'0020'0000ull)

/**
 * @brief 构造TCB能力
 *
 * @param p    在p内构造一个TCB能力
 * @param tcb  构造的能力指向tcb
 * @return CapIdx 能力索引
 */
CapIdx create_tcb_cap(PCB *p, TCB *tcb);

/**
 * @brief 从源进程的源能力派生一个新的TCB能力到目标进程
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @param priv 新权限
 * @return CapIdx 新的能力索引
 */
CapIdx tcb_cap_derive(PCB *sproc, CapIdx sidx, PCB *dproc, qword priv);

/**
 * @brief 从源进程的源能力派生一个新的TCB能力到目标进程的指定位置
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @param didx 目标能力索引位置
 * @param priv 新权限
 * @return CapIdx 新的能力索引
 */
CapIdx tcb_cap_derive_at(PCB *sproc, CapIdx sidx, PCB *dproc, CapIdx didx,
                         qword priv);

/**
 * @brief 从源进程的源能力克隆一个新的TCB能力到目标进程
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @return CapIdx 新的能力索引
 */
CapIdx tcb_cap_clone(PCB *sproc, CapIdx sidx, PCB *dproc);

/**
 * @brief 从源进程的源能力克隆一个新的TCB能力到目标进程的指定位置
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @param didx 目标能力索引位置
 * @return CapIdx 新的能力索引
 */
CapIdx tcb_cap_clone_at(PCB *sproc, CapIdx sidx, PCB *dproc, CapIdx didx);

/**
 * @brief 将能力降级为更低权限的能力
 *
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @param cap_priv 新的能力权限
 * @return CapIdx 降级后的能力索引(和idx相同)
 */
CapIdx tcb_cap_degrade(PCB *p, CapIdx idx, qword cap_priv);

/**
 * @brief 移除TCB能力
 *
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @return CapIdx 被移除的能力索引
 */
CapIdx tcb_cap_remove(PCB *p, CapIdx idx);

/**
 * @brief 解包TCB能力, 获得TCB指针
 *
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @return TCB* TCB指针
 */
TCB *tcb_cap_unpack(PCB *p, CapIdx idx);

//===========

/**
 * @brief 将线程切换到yield状态
 *
 * @param p 当前进程的PCB
 * @param idx 能力索引
 */
void tcb_cap_yield(PCB *p, CapIdx idx);

#define TCB_CAP_START(proc, idx, cap, tcb, priv_check, ret_val) \
    Capability *cap = fetch_cap(proc, idx);                     \
    if (cap == nullptr) {                                           \
        log_error("%s:指针指向的能力不存在!", __FUNCTION__);        \
        return ret_val;                                             \
    }                                                               \
    if (cap->type != CAP_TYPE_TCB) {                                \
        log_error("%s:该能力不为TCB能力!", __FUNCTION__);           \
        return ret_val;                                             \
    }                                                               \
    if (cap->cap_data == nullptr) {                                 \
        log_error("%s:能力数据为空!", __FUNCTION__);                \
        return ret_val;                                             \
    }                                                               \
    TCB *tcb = (TCB *)cap->cap_data;                                \
    if (!derivable(cap->cap_priv, priv_check)) {                    \
        log_error("%s:能力权限不足!", __FUNCTION__);                \
        return ret_val;                                             \
    }
