/**
 * @file vfs.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtual File System
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/block.h>
#include <object/shared.h>
#include <sus/map.h>
#include <sus/owner.h>
#include <sus/path.h>
#include <sustcore/errcode.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <string>

class VFile : public SharedObject<VFile> {
private:
    IFsDriver *_ifs;
    ISuperblock *_isb;
    IFile *_ifile;
    bool _closable;

public:
    static constexpr PayloadType IDENTIFIER = PayloadType::VFILE;
    constexpr VFile(IFsDriver *ifs, ISuperblock *isb, IFile *ifile)
        : _ifs(ifs), _isb(isb), _ifile(ifile), _closable(false) {}
    constexpr virtual ~VFile() {}
    virtual void on_death(void) override {
        _closable = true;
    }
    constexpr bool closable(void) const {
        return _closable;
    }
    constexpr IFsDriver *fs_driver() const {
        return _ifs;
    }
    constexpr ISuperblock *superblock() const {
        return _isb;
    }
    constexpr IFile *ifile() const {
        return _ifile;
    }
    // operations
    friend class VFS;
};

class VFileAccessor : public SharedObjectAccessor<VFile> {
public:
    using Base                              = SharedObjectAccessor<VFile>;
    using Payload                           = Base::Payload;
    static constexpr PayloadType IDENTIFIER = Base::IDENTIFIER;

public:
    constexpr VFileAccessor(VFile *file) : Base(file) {}
    virtual ~VFileAccessor() {}
};

enum class MountFlags { NONE = 0 };

class VFS {
private:
    util::LinkedMap<std::string, util::owner<IFsDriver *>> fs_table;
    util::LinkedMap<util::Path, util::owner<ISuperblock *>> mount_table;
    util::LinkedMap<util::Path, util::owner<VFile *>> openlist;
public:
    VFS()  = default;
    ~VFS() = default;

    VFS(const VFS &)            = delete;
    VFS &operator=(const VFS &) = delete;
    VFS(VFS &&)                 = delete;
    VFS &operator=(VFS &&)      = delete;

    // 注册文件系统
    Result<void> register_fs(util::owner<IFsDriver *> &&driver);
    Result<void> unregister_fs(const char *fs_name);
    // 挂载文件系统
    Result<void> mount(const char *fs_name, IBlockDevice *device,
                       const char *mountpoint, MountFlags flags,
                       const char *options);
    Result<void> umount(const char *mountpoint);
    // 打开文件
    Result<util::owner<VFileAccessor *>> open(const char *filepath);
    // 关闭文件不需要额外接口, 直接delete VFileAccessor即可
    // 不过, 仍然在此处预留这样一个接口, 帮助你delete(
    constexpr Result<void> close(util::owner<VFileAccessor *> &&file_acc) {
        delete file_acc;
        void_return();
    }

    // 整理openlist
    Result<void> tidy_up();
};