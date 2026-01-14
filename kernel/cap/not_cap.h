/**
 * @file not_cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通知能力
 * @version alpha-1.0.0
 * @date 2025-12-23
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <cap/capability.h>

// Notification被定义为一个长256个位的位图
// 每个位代表一个不同的通知类型
// Notification能力也与TCB, PCB能力有所联动
// 具体来说, 当进程/线程具有挂起自身的能力, 且具有检查某个通知ID的能力时
// 进程/线程就可以将自身挂起, 直到该通知ID被设置
// 进一步地, 进程/线程可以等待一类通知ID的集合
// 只要集合中的任意一个通知ID被设置, 进程/线程就会被唤醒
// 这种设计允许进程/线程高效地等待多种事件的发生
// 而不需要为每个事件单独创建等待机制
// (可以理解成linux中的SIGNAL机制
// 但一方面, 每个进程都有且仅有一个SIGNAL位图,
// 而Notification能力可以被多个进程/线程持有,
// 而一个进程也可以持有多个Notification能力,
// 另一方面, Notification能力的权限可以被更细粒度地控制,
// 例如某个进程只能设置某些通知ID, 而不能设置其他通知ID,
// 同时也可以创建多个Notification能力, 隔离不同的通知用途)

//====================================================

// 通知能力数据
typedef struct {
    int notif_id;
    qword bitmap[NOTIFICATION_BITMAP_QWORDS];  // 通知位图
} Notification;

//====================================================

// 通知能力权限
typedef struct {
    qword priv_set[NOTIFICATION_BITMAP_QWORDS];  // 设置通知的权限位图
    qword priv_reset[NOTIFICATION_BITMAP_QWORDS];  // 清除通知的权限位
    qword priv_check[NOTIFICATION_BITMAP_QWORDS];  // 检查通知的权限位
} NotCapPriv;

// Notification附加权限 - 全部权限
extern const NotCapPriv NOTIFICATION_ALL_PRIV;
// Notification附加权限 - 无权限
extern const NotCapPriv NOTIFICATION_NONE_PRIV;

/**
 * @brief 设置设置通知位权限
 *
 * @param priv 通知能力权限指针
 * @param nid 通知ID
 * @return NotCapPriv* 设置后的权限指针
 */
NotCapPriv *not_priv_set(NotCapPriv *priv, int nid);

/**
 * @brief 设置清除通知位权限
 *
 * @param priv 通知能力权限指针
 * @param nid 通知ID
 * @return NotCapPriv* 设置后的权限指针
 */
NotCapPriv *not_priv_reset(NotCapPriv *priv, int nid);

/**
 * @brief 设置检查通知位权限
 *
 * @param priv 通知能力权限指针
 * @param nid 通知ID
 * @return NotCapPriv* 设置后的权限指针
 */
NotCapPriv *not_priv_check(NotCapPriv *priv, int nid);

/**
 * @brief 通知附加权限派生检查
 *
 * @param parent_priv 通知能力父权限
 * @param child_priv 通知能力子权限
 * @return true 可派生
 * @return false 不可派生
 */
bool notification_derivable(const NotCapPriv *parent_priv,
                            const NotCapPriv *child_priv);

/**
 * @brief 构造Notification能力
 *
 * @param p 在p内构造一个Notification能力
 * @return  CapIdx 能力索引
 */
CapIdx create_notification_cap(PCB *p);

/**
 * @brief 从sproc的sidx能力派生一个新的Notification能力到dproc
 *
 * @param sproc   源进程
 * @param sidx 源能力
 * @param dproc   目标进程
 * @param cap_priv    新权限
 * @param notif_priv 新通知权限
 * @return CapIdx 派生出的能力索引
 */
CapIdx not_cap_derive(PCB *sproc, CapIdx sidx, PCB *dproc,
                      qword cap_priv,
                      NotCapPriv *notif_priv);

/**
 * @brief 从sproc的sidx能力派生一个新的Notification能力到dproc
 *
 * @param sproc   源进程
 * @param sidx 源能力
 * @param dproc   目标进程
 * @param dproc 目标能力索引位置
 * @param cap_priv    新权限
 * @param notif_priv 新通知权限
 * @return CapIdx 派生出的能力索引
 */
CapIdx not_cap_derive_at(PCB *sproc, CapIdx sidx, PCB *dproc, CapIdx didx,
                         qword cap_priv,
                         NotCapPriv *notif_priv);

/**
 * @brief 从sproc的sidx能力克隆一个Notification能力到dproc
 *
 * @param sproc   源进程
 * @param sidx 源能力
 * @param dproc   目标进程
 * @return CapIdx 派生出的能力索引
 */
CapIdx not_cap_clone(PCB *sproc, CapIdx sidx, PCB *dproc);

/**
 * @brief 从sproc的sidx能力克隆一个Notification能力到dproc
 *
 * @param sproc   源进程
 * @param sidx 源能力
 * @param dproc   目标进程
 * @param dproc 目标能力索引位置
 * @return CapIdx 派生出的能力索引
 */
CapIdx not_cap_clone_at(PCB *sproc, CapIdx sidx, PCB *dproc, CapIdx didx);

/**
 * @brief 将能力降级为更低权限的能力
 *
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @param cap_priv 新的能力权限
 * @param notif_priv 新的通知权限
 * @return CapIdx 降级后的能力索引(和idx相同)
 */
CapIdx not_cap_degrade(PCB *p, CapIdx idx,
                       qword cap_priv,
                       NotCapPriv *notif_priv);

/**
 * @brief 解包Notification能力, 获取其数据指针
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @return Notification* 通知数据指针
 */
Notification *not_cap_unpack(PCB *p, CapIdx idx);

//====================================================

/**
 * @brief 设置通知位
 *
 * @param p 当前进程PCB指针
 * @param idx 能力索引
 * @param nid 通知ID (0-255)
 */
void not_cap_set(PCB *p, CapIdx idx, int nid);

/**
 * @brief 清除通知位
 *
 * @param p 当前进程PCB指针
 * @param idx 通知能力索引
 * @param nid 通知ID (0-255)
 */
void not_cap_reset(PCB *p, CapIdx idx, int nid);

/**
 * @brief 检查通知位
 *
 * @param p 当前进程PCB指针
 * @param idx 通知能力索引
 * @param nid 通知ID (0-255)
 * @return true 通知已设置
 * @return false 通知未设置
 */
bool not_cap_check(PCB *p, CapIdx idx, int nid);

/**
 * @brief 等待通知
 *
 * @param p 当前进程PCB指针
 * @param tcb_cap TCB能力索引
 * @param not_cap 通知能力索引
 * @param wait_bitmap 等待位图(应当总长256位)
 * @return true 通知已到达
 * @return false 通知未到达
 */
bool tcb_cap_wait_notification(PCB *p, CapIdx tcb_idx, CapIdx not_idx,
                               qword *wait_bitmap);

#define NOT_CAP_START(proc, idx, cap, notif, cap_priv_check, \
                      notif_priv_check, ret_val)                            \
    /** 获取能力 */                                                     \
    Capability *cap = fetch_cap(proc, idx);                             \
    if (cap == nullptr) {                                                   \
        log_error("%s:指针指向的能力不存在!", __FUNCTION__);                \
        return ret_val;                                                     \
    }                                                                       \
    if (cap->type != CAP_TYPE_NOT) {                                        \
        log_error("%s:该能力不为Notification能力!", __FUNCTION__);          \
        return ret_val;                                                     \
    }                                                                       \
    if (cap->cap_data == nullptr) {                                         \
        log_error("%s:能力数据为空!", __FUNCTION__);                        \
        return ret_val;                                                     \
    }                                                                       \
    Notification *notif = (Notification *)cap->cap_data;                    \
    /** 检验权限 */                                                     \
    if (cap->attached_priv == nullptr) {                                    \
        log_error("%s:能力权限数据为空!", __FUNCTION__);                    \
        return ret_val;                                                     \
    }                                                                       \
    if (!derivable(cap->cap_priv, cap_priv_check)) {                        \
        log_error("%s:能力权限不包含所需权限!", __FUNCTION__);              \
        return ret_val;                                                     \
    }                                                                       \
    if (!notification_derivable((NotCapPriv *)cap->attached_priv,           \
                                notif_priv_check))                          \
    {                                                                       \
        log_error("%s:能力通知权限不包含所需权限!", __FUNCTION__);          \
        return ret_val;                                                     \
    }                                                                       \
    /** 处理函数主体部分 */
