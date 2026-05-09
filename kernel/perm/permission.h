/**
 * @file permission.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 权限
 * @version alpha-1.0.0
 * @date 2026-02-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <perm/perm.h>
#include <sus/types.h>

namespace perm {
    // 权限位, 低16位保留给基础能力使用, 剩余48位供派生能力使用
    constexpr bool imply(b64 owned, b64 required) noexcept {
        return BITS_IMPLIES(owned, required);
    }

    constexpr b64 allperm() noexcept {
        return ~0ULL;
    }
}  // namespace perm
