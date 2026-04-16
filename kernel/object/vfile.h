/**
 * @file vfile.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief virtual file object
 * @version alpha-1.0.0
 * @date 2026-04-17
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <vfs/vfs.h>

// VFileAccessor(vfs/vfs.h) 是 Capability 所持有的对象
// 其在销毁时将会释放对 VFile 对象的引用计数
// 而 VFile 对象的生命周期由 VFS 控制
// VFileAccessor 的生命周期由 CHolder 控制
// 如此, VFileAccessor 作为 Payload 的生命周期就与 Capability 的生命周期绑定
// 避免让 Capability 系统本身考虑 SharedObject 的生命周期, 以简化 Capability 系统的设计
class VFileOperator {
protected:
    Capability *_cap;
    VFileAccessor *_acc;
    VINode *_file;
    template <b64 perm>
    bool imply() const {
        return _cap->perm().basic_imply(perm);
    }

public:
    constexpr VFileOperator(Capability *cap)
        : _cap(cap), _acc(cap->payload<VFileAccessor>()), _file(_acc->obj()) {}
    ~VFileOperator() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    Result<size_t> read(off_t offset, void *buf, size_t len);
    Result<size_t> write(off_t offset, const void *buf, size_t len);
    Result<size_t> size();
    Result<void> sync();
};