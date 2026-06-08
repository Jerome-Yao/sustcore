/**
 * @file vdir.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief virtual directory object
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <vfs/vfs.h>

namespace cap {
    class VDirectoryObject : public CapObj<::VDirectory> {
    public:
        explicit VDirectoryObject(util::nonnull<Capability *> cap)
            : CapObj<::VDirectory>(cap) {}
        ~VDirectoryObject() = default;

        void *operator new(size_t size) = delete;
        void operator delete(void *ptr) = delete;

        [[nodiscard]]
        Result<void> sync();
    };
}  // namespace cap
