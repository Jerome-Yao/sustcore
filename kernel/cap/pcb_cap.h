/**
 * @file pcb_cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief PCB能力相关操作
 * @version alpha-1.0.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <cap/capability.h>
#include <task/task_struct.h>

// 退出进程权限
#define PCB_PRIV_EXIT          (0x0000'0000'0001'0000ull)
// fork新进程权限
#define PCB_PRIV_FORK          (0x0000'0000'0002'0000ull)
// 获取PID权限
#define PCB_PRIV_GETPID        (0x0000'0000'0004'0000ull)
// 创建线程权限
#define PCB_PRIV_CREATE_THREAD (0x0000'0000'0008'0000ull)
// 遍历能力权限
#define PCB_PRIV_ENUM_CAPS     (0x0000'0000'0010'0000ull)
// 迁移能力权限
#define PCB_PRIV_MIGRATE_CAPS  (0x0000'0000'0020'0000ull)

/**
 * @brief 构造PCB能力
 *
 * @param p 需要构造PCB能力的进程
 * @return CapIdx 能力索引
 */
CapIdx create_pcb_cap(PCB *p);

/**
 * @brief 从源进程的源能力派生一个新的PCB能力到目标进程
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @param priv 新权限
 * @return CapIdx 新的能力索引
 */
CapIdx pcb_cap_derive(PCB *sproc, CapIdx sidx, PCB *dproc, qword priv);

/**
 * @brief 从源进程的源能力派生一个新的PCB能力到目标进程的指定位置
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @param didx 目标能力索引位置
 * @param priv 新权限
 * @return CapIdx 新的能力索引
 */
CapIdx pcb_cap_derive_at(PCB *sproc, CapIdx sidx, PCB *dproc, CapIdx didx,
                         qword priv);

/**
 * @brief 从源进程的源能力克隆一个PCB能力到目标进程
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @param priv 新权限
 * @return CapIdx 新的能力索引
 */
CapIdx pcb_cap_clone(PCB *sproc, CapIdx sidx, PCB *dproc);

/**
 * @brief 从源进程的源能力克隆一个PCB能力到目标进程的指定位置
 *
 * @param sproc 源进程
 * @param sidx 源能力
 * @param dproc 目标进程
 * @param didx 目标能力索引位置
 * @return CapIdx 新的能力索引
 */
CapIdx pcb_cap_clone_at(PCB *sproc, CapIdx sidx, PCB *dproc, CapIdx didx);

/**
 * @brief 将能力降级为更低权限的能力
 *
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @param cap_priv 新的能力权限
 * @return CapIdx 降级后的能力索引(和idx相同)
 */
CapIdx pcb_cap_degrade(PCB *p, CapIdx idx, qword cap_priv);

/**
 * @brief 解包PCB能力, 获得PCB指针
 *
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @return PCB* PCB指针
 */
PCB *pcb_cap_unpack(PCB *p, CapIdx idx);

/**
 * @brief 退出进程
 *
 * @param p 当前进程的PCB
 * @param idx 能力索引
 */
void pcb_cap_exit(PCB *p, CapIdx idx);

/**
 * @brief fork一个新进程
 *
 * @param p 当前进程的PCB
 * @param idx 能力索引
 * @param child_cap 返回新进程的能力索引
 * @return PCB* 新进程的PCB指针
 */
PCB *pcb_cap_fork(PCB *p, CapIdx idx, CapIdx *child_cap);

/**
 * @brief 获取进程的PID
 *
 * @param p 当前进程的PCB
 * @param idx 能力索引
 * @return pid_t 进程的PID
 */
pid_t pcb_cap_getpid(PCB *p, CapIdx idx);

/**
 * @brief 在idx所指向的进程创建一个新线程
 *
 * @param p 当前进程的PCB
 * @param idx 能力索引
 * @param entrypoint 线程入口点
 * @param priority 线程优先级
 * @return CapIdx 新线程的能力索引
 */
CapIdx pcb_cap_create_thread(PCB *p, CapIdx idx, void *entrypoint,
                             int priority);

#define PCB_CAP_START(proc, idx, cap, pcb, priv_check, ret_val) \
    Capability *cap = fetch_cap(proc, idx);                     \
    if (cap == nullptr) {                                           \
        log_error("%s:指针指向的能力不存在!", __FUNCTION__);        \
        return ret_val;                                             \
    }                                                               \
    if (cap->type != CAP_TYPE_PCB) {                                \
        log_error("%s:该能力不为PCB能力!", __FUNCTION__);           \
        return ret_val;                                             \
    }                                                               \
    if (cap->cap_data == nullptr) {                                 \
        log_error("%s:能力数据为空!", __FUNCTION__);                \
        return ret_val;                                             \
    }                                                               \
    PCB *pcb = (PCB *)cap->cap_data;                                \
    if (!derivable(cap->cap_priv, priv_check)) {                    \
        log_error("%s:能力权限不足!", __FUNCTION__);                \
        return ret_val;                                             \
    }
