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
// 避免让 Capability 系统本身考虑 SharedObject 的生命周期, 以简化 Capability
// 系统的设计
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

    /**
     * @brief 读取一定长度的数据到缓冲区
     * 
     * @param offset 偏移
     * @param buf 缓冲区
     * @param len 需要读取的字节数
     * @return Result<size_t> 实际读取的字节数. 当实际读取的字节数不等于需要读取的字节数时, 将会返回一个错误码
     */
    Result<size_t> read_exact(off_t offset, void *buf, size_t len) {
        auto read_res = read(offset, buf, len);
        if (!read_res.has_value()) {
            propagate_return(read_res);
        }
        size_t got = read_res.value();
        if (got != len) {
            unexpect_return(ErrCode::IO_ERROR);
        }
        return got;
    }
};