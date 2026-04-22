/**
 * @file cholder.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力持有者
 * @version alpha-1.0.0
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <cap/cspace.h>
#include <object/csa.h>
#include <sustcore/capability.h>

CHolder::CHolder(size_t cholder_id)
    : _space(this),
            _csa_idx(capidx::make(0, 0)),
      cholder_id(cholder_id) {
    auto ret= _space.create<CSpaceAccessor>(_csa_idx, &_space);
    assert(ret.has_value());
}

CHolder::~CHolder() {}

Result<Capability *> CHolder::access(CapIdx idx) {
    if (!capidx::valid(idx)) {
        return {unexpect, ErrCode::TYPE_NOT_MATCHED};
    }

   return _space.get(idx);
}