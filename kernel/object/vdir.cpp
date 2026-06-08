/**
 * @file vdir.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtual Directory Object
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <object/perm.h>
#include <object/vdir.h>
#include <vfs/vfs.h>

namespace cap {
    Result<void> VDirectoryObject::sync() {
        using namespace perm::vdir;
        if (!imply(WRITE)) {
            loggers::CAPABILITY::ERROR("权限不足");
            return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
        }
        return VFS::inst().sync(*_obj);
    }
}  // namespace cap
