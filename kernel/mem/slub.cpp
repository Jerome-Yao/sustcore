/**
 * @file slub.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * theflysong(song_of_the_fly@163.com)
 * @brief SLUB Allocator
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/slub.h>

namespace slub {
    SlubAllocator<MixedSizeAllocator::AllocRecord>
        MixedSizeAllocator::ALLOC_RECORD_SLUB;
    util::IntrusiveList<MixedSizeAllocator::AllocRecord>
        MixedSizeAllocator::alloc_records;

    void *MixedSizeAllocator::AllocRecord::operator new(size_t sz) {
        assert(sz == sizeof(AllocRecord));
        return ALLOC_RECORD_SLUB.alloc();
    }
    void MixedSizeAllocator::AllocRecord::operator delete(void *ptr) {
        ALLOC_RECORD_SLUB.free(static_cast<AllocRecord *>(ptr));
    }
}  // namespace slub
