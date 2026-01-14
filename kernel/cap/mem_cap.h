/**
 * @file mem_cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内存能力
 * @version alpha-1.0.0
 * @date 2026-01-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <stddef.h>

typedef struct {
    // 内存指针
    void *mem_paddr;
    // 内存大小
    size_t mem_size;
    // 是否为共享内存
    bool shared;
    // 是否为内存映射IO区域
    bool mmio;
    // 是否为通过pmm分配的内存
    // 当该项为真时
    bool allocated;
} MemoryData;

// 内存能力 - 通用权限
#define MEM_CAP_PRIV_GETPADDR (0x0000'0000'0001'0000ull)
#define MEM_CAP_PRIV_MAP      (0x0000'0000'0002'0000ull)
#define MEM_CAP_PRIV_UNMAP    (0x0000'0000'0004'0000ull)
#define MEM_CAP_PRIV_READ     (0x0000'0000'0008'0000ull)
#define MEM_CAP_PRIV_WRITE    (0x0000'0000'0010'0000ull)
#define MEM_CAP_PRIV_EXEC     (0x0000'0000'0020'0000ull)

// 内存能力 - 共享内存权限
#define SHM_CAP_PRIV_SHARE   (0x0000'0010'0000'0000ull)
#define SHM_CAP_PRIV_UNSHARE (0x0000'0020'0000'0000ull)

/**
 * @brief 为指定内存创建内存能力
 *
 * @param p 内存能力所属进程
 * @param paddr 内存物理地址
 * @param size 内存大小
 * @param shared 是否为共享内存
 * @param mmio 是否为内存映射IO区域
 * @param allocated 是否是kmalloc分配的内存
 * @return CapIdx 能力索引
 */
CapIdx mem_cap_create(PCB *p, void *paddr, size_t size, bool shared, bool mmio,
                      bool allocated);

/**
 * @brief 分配一块内存并为其创建内存能力
 *
 * 该函数会分配一块指定大小的内存, 并为这块内存创建一个内存能力.
 *
 * @param p 内存能力所属进程
 * @param size 内存大小
 * @param shared 是否为共享内存
 * @return CapIdx 能力索引
 */
CapIdx mem_cap_alloc_and_create(PCB *p, size_t size, bool shared);

/**
 * @brief 从源进程的源能力派生一个新的内存能力到目标进程
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力索引
 * @param dst_p 目标进程
 * @param priv 新权限
 * @return CapIdx 新的能力索引
 */
CapIdx mem_cap_derive(PCB *src_p, CapIdx src_ptr, PCB *dst_p, qword priv);

#define MEM_CAP_START(proc, idx, cap, mem, priv_check, ret_val) \
    Capability *cap = fetch_cap(proc, idx);                     \
    if (cap == nullptr) {                                           \
        log_error("%s:指针指向的能力不存在!", __FUNCTION__);        \
        return ret_val;                                             \
    }                                                               \
    if (cap->type != CAP_TYPE_MEM) {                                \
        log_error("%s:该能力不为MEM能力!", __FUNCTION__);           \
        return ret_val;                                             \
    }                                                               \
    if (cap->cap_data == nullptr) {                                 \
        log_error("%s:能力数据为空!", __FUNCTION__);                \
        return ret_val;                                             \
    }                                                               \
    MemoryData *pcb = (MemoryData *)cap->mem;                       \
    if (!derivable(cap->cap_priv, priv_check)) {                    \
        log_error("%s:能力权限不足!", __FUNCTION__);                \
        return ret_val;                                             \
    }
