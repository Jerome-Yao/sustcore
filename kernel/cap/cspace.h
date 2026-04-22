/**
 * @file cspace.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability空间
 * @version alpha-1.0.0
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <expected>

// CSpace
// 每个CSpace都含有若干个CGroup
// 然而这些CGroup并不一定一开始都被创建
// 只有被使用时, CGroup才会被创建
class CSpace {
protected:
    CGroup *_groups[CSPACE_SIZE];
    CHolder *_holder;

    inline CGroup *group_at(size_t group_idx) {
        assert(group_idx < CSPACE_SIZE);
        if (_groups[group_idx] == nullptr) {
            _groups[group_idx] = new CGroup();
        }

        return _groups[group_idx];
    }

public:
    const int sp_idx;
    CSpace(CHolder *holder);
    ~CSpace();

    constexpr CHolder *holder(void) const {
        return _holder;
    }

    template <typename PayloadType, typename... Args>
    Result<void> create(CapIdx idx, Args &&...args) {
        const size_t group_idx = capidx::group(idx);
        if (group_idx >= CSPACE_SIZE) {
            loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx,
                              this->sp_idx);
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        CGroup *group = group_at(group_idx);
        return group->create<PayloadType>(idx,
                                          std::forward<Args>(args)...);
    }

    template <typename PayloadType>
    Result<void> create_from(CapIdx idx, util::owner<Payload *> payload) {
        const size_t group_idx = capidx::group(idx);
        if (group_idx >= CSPACE_SIZE) {
            loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx,
                              this->sp_idx);
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        CGroup *group = group_at(group_idx);
        return group->create_from<PayloadType>(idx, payload);
    }

    Result<void> clone(CapIdx idx, Capability *parent);
    Result<void> migrate(CapIdx idx, Capability *origin);
    Result<void> remove(CapIdx idx);

    // get group
    Result<CGroup *> group(CapIdx idx);
    Result<Capability *> get(CapIdx idx);

    constexpr bool empty(void) const {
        bool flag = false;
        for (size_t i = 0; i < CSPACE_SIZE; i++) {
            flag |= (_groups[i] != nullptr);
        }
        return !flag;
    }
    // 腾出CSpace中所有为空的CGroup
    void tidyup(void);

    friend class CSAOperator;
};