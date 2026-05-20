/**
 * @file cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>
#include <sustcore/capability.h>

namespace syscall {
    Result<CapIdx> cap_clone(CapIdx src);
    Result<bool> cap_downgrade(CapIdx idx, b64 new_perm);
    Result<CapIdx> cap_derive(CapIdx src, b64 new_perm);
    Result<bool> sys_cap_lookup(CapIdx idx, VirAddr info_uaddr);
    Result<bool> cap_remove(CapIdx idx);
}  // namespace syscall
