/**
 * @file cap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <env.h>
#include <logger.h>
#include <object/memory.h>
#include <sustcore/addr.h>
#include <sustcore/capability.h>
#include <syscall/cap.h>
#include <syscall/uaccess.h>

namespace syscall {
    Result<CapIdx> cap_clone(CapIdx src) {
        auto clone_res = cap::CHolder::clone(src);
        propagate(clone_res);
        return clone_res.value();
    }

    Result<bool> cap_downgrade(CapIdx idx, b64 new_perm) {
        auto downgrade_res = cap::CHolder::downgrade(idx, new_perm);
        propagate(downgrade_res);
        return true;
    }

    Result<CapIdx> cap_derive(CapIdx src, b64 new_perm) {
        auto derive_res = cap::CHolder::derive(src, new_perm);
        propagate(derive_res);
        return derive_res.value();
    }

    Result<bool> sys_cap_lookup(CapIdx idx, VirAddr info_uaddr) {
        if (!info_uaddr.nonnull()) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto cap_res = cap::CHolder::lookup(idx);
        if (!cap_res.has_value()) {
            if (cap_res.error() == ErrCode::OUT_OF_BOUNDARY) {
                return false;
            }
            propagate_return(cap_res);
        }

        // 将能力类型与权限回填到用户态缓冲区
        CapInfo info{
            .type        = cap_res.value()->payload()->type_id(),
            .permissions = cap_res.value()->perm(),
        };
        UBuffer info_buf(info_uaddr, sizeof(info));
        memcpy(info_buf.kbuf(), &info, sizeof(info));
        info_buf.sync_to_user();
        return true;
    }

    Result<bool> cap_remove(CapIdx idx) {
        auto cap_res = cap::CHolder::lookup(idx);
        propagate(cap_res);
        auto *memory = cap_res.value()->payload_as<cap::MemoryPayload>();
        auto *tmm    = env::inst().tmm();
        if (memory != nullptr && tmm != nullptr &&
            tmm->has_memory_mapping(memory))
        {
            unexpect_return(ErrCode::BUSY);
        }
        auto remove_res = cap::CHolder::remove(idx);
        propagate(remove_res);
        return true;
    }
}  // namespace syscall
