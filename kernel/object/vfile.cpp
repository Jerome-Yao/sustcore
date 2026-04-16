/**
 * @file vfile.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtual File Object
 * @version alpha-1.0.0
 * @date 2026-04-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <object/vfile.h>
#include <perm/vfile.h>
#include <env.h>

Result<size_t> VFileOperator::read(off_t offset, void *buf, size_t len)
{
    using namespace perm::vfile;
    if (!imply<READ>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // Get VFS from Environment
    auto &vfs = env::inst().vfs();
    // 调用VFS的read接口
    return vfs.read(_file, offset, buf, len);
}


Result<size_t> VFileOperator::write(off_t offset, const void *buf, size_t len)
{
    using namespace perm::vfile;
    if (!imply<WRITE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // Get VFS from Environment
    auto &vfs = env::inst().vfs();
    // 调用VFS的write接口
    return vfs.write(_file, offset, buf, len);
}

Result<size_t> VFileOperator::size()
{
    using namespace perm::vfile;
    if (!imply<READ>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // Get VFS from Environment
    auto &vfs = env::inst().vfs();
    // 调用VFS的size接口
    return vfs.size(_file);
}

Result<void> VFileOperator::sync()
{
    using namespace perm::vfile;
    if (!imply<WRITE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // Get VFS from Environment
    auto &vfs = env::inst().vfs();
    // 调用VFS的sync接口
    return vfs.sync(_file);
}