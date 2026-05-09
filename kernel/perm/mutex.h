/**
 * @file mutex.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Mutex Permissions
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>

namespace perm::mutex {
    // Mutex的权限定义
    // 使用互斥锁( lock() / unlock() )的权限
    constexpr b64 USE     = 0x01'0000;
    // 查询锁状态的权限
    constexpr b64 QUERY   = 0x02'0000;
    // 销毁锁的权限
    constexpr b64 DESTROY = 0x04'0000;
}  // namespace perm::mutex