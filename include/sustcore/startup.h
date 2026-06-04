/**
 * @file startup.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 共享的用户进程启动参数定义
 * @version alpha-1.0.0
 * @date 2026-06-05
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>
#include <sustcore/capability.h>

namespace task {
    /**
     * @brief 用户进程启动时写入主栈的启动参数块.
     *
     * 该结构由内核写入用户栈, 并由 libkmod 在 `_start` 后解析. 
     */
    struct StartupInfo {
        VirAddr heap_vaddr;    ///< 堆起始虚拟地址.
        VirAddr stack_vaddr;   ///< 用户栈起始虚拟地址.
        VirAddr entrypoint;    ///< 用户入口地址.
        CapIdx pcb_cap;        ///< 自身 PCB capability.
        CapIdx main_tcb_cap;   ///< 主线程 TCB capability.
        CapIdx heap_mem_cap;   ///< 堆 Memory capability.
        CapIdx stack_mem_cap;  ///< 主栈 Memory capability.
    };
}  // namespace task
