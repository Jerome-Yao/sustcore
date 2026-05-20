/**
 * @file memory.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Memory capability syscalls
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <cap/permission.h>
#include <env.h>
#include <logger.h>
#include <mem/gfp.h>
#include <mem/vma.h>
#include <object/memory.h>
#include <syscall/memory.h>
#include <syscall/uaccess.h>

#include <cstring>

namespace {
    /**
     * @brief 查找当前进程 capability 空间中的 Memory payload. 
     */
    Result<cap::MemoryPayload *> lookup_memory(
        CapIdx idx, cap::Capability **out_cap = nullptr) {
        auto cap_res = cap::CHolder::lookup(idx);
        propagate(cap_res);
        if (out_cap != nullptr) {
            *out_cap = cap_res.value();
        }
        auto *memory = cap_res.value()->payload_as<cap::MemoryPayload>();
        if (memory == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        return memory;
    }

}  // namespace

namespace syscall {
    Result<CapIdx> mem_create(size_t memsz, bool shared, bool continuity,
                              cap::MemoryGrowth growth) {
        if (shared && growth != cap::MemoryGrowth::FIXED) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto payload =
            new cap::MemoryPayload(memsz, shared, continuity, growth);
        auto insert_res =
            cap::CHolder::current().and_then([&](cap::CHolder *holder) {
                return holder->internal_insert_to_free(payload);
            });
        if (!insert_res.has_value()) {
            delete payload;
            propagate_return(insert_res);
        }
        return insert_res.value();
    }

    Result<bool> mem_unmap(CapIdx idx, VirAddr vaddr) {
        cap::Capability *cap = nullptr;
        auto memory_res      = lookup_memory(idx, &cap);
        propagate(memory_res);
        auto *tmm = env::inst().tmm();
        if (tmm == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        cap::MemoryObject obj(util::nnullforce(cap));
        auto remove_res = obj.unmap_from(*tmm, vaddr);
        propagate(remove_res);
        return true;
    }

    Result<bool> mem_resize(CapIdx idx, size_t newsz) {
        cap::Capability *cap = nullptr;
        auto memory_res      = lookup_memory(idx, &cap);
        propagate(memory_res);
        cap::MemoryObject obj(util::nnullforce(cap));
        auto *tmm       = env::inst().tmm();
        auto resize_res = obj.resize_in(tmm, newsz);
        propagate(resize_res);
        return true;
    }

    Result<void> mem_query(CapIdx idx, VirAddr out_uaddr) {
        if (!out_uaddr.nonnull()) {
            unexpect_return(ErrCode::NULLPTR);
        }
        cap::Capability *cap = nullptr;
        auto memory_res      = lookup_memory(idx, &cap);
        propagate(memory_res);
        cap::MemoryObject obj(util::nnullforce(cap));
        auto query_res = obj.query();
        propagate(query_res);

        MemQueryRet ret{query_res.value().memsz, query_res.value().allocated};
        UBuffer out_buf(out_uaddr, sizeof(ret));
        memcpy(out_buf.kbuf(), &ret, sizeof(ret));
        out_buf.sync_to_user();
        void_return();
    }
}  // namespace syscall
