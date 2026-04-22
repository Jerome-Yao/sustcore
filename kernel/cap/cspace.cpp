/**
 * @file cspace.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability空间
 * @version alpha-1.0.0
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cap/cspace.h>
#include <cap/cholder.h>
#include <sustcore/capability.h>



// CSpace
CSpace::CSpace(CHolder *holder) :  _holder(holder), sp_idx(0) {
    memset(_groups, 0, sizeof(_groups));
}

CSpace::~CSpace() {
    for (size_t i = 0; i < CSPACE_SIZE; i++) {
        if (_groups[i]) {
            delete _groups[i];
        }
    }
}

Result<void> CSpace::clone(CapIdx idx, Capability *parent) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    CGroup *group = group_at(group_idx);
    return group->clone(idx, parent);
}

Result<void> CSpace::migrate(CapIdx idx, Capability *origin) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    CGroup *group = group_at(group_idx);
    return group->migrate(idx, origin);
}

Result<void> CSpace::remove(CapIdx idx) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_groups[group_idx]) {
        loggers::CAPABILITY::ERROR("CGroup索引%u在CSpace %d中未被创建", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    return _groups[group_idx]->remove(idx);
}

Result<CGroup *> CSpace::group(CapIdx idx) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_groups[group_idx]) {
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
    }
    return _groups[group_idx];
}

Result<Capability *> CSpace::get(CapIdx idx) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_groups[group_idx]) {
        loggers::CAPABILITY::ERROR("CGroup索引%u在CSpace %d中未被创建", group_idx, sp_idx);
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
    }
    return _groups[group_idx]->get(idx);
}

void CSpace::tidyup(void) {
    for (size_t i = 0; i < CSPACE_SIZE; i++) {
        if (_groups[i] && _groups[i]->empty()) {
            delete _groups[i];
            _groups[i] = nullptr;
        }
    }
}