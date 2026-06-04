/**
 * @file vfs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <sus/path.h>
#include <sustcore/errcode.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cassert>
#include <cstring>
#include <utility>

namespace {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static VFS inst_vfs;
    static bool inst_vfs_initialized = false;
}  // namespace

VFile::VFile(util::owner<VINode *> vind, const util::Path &mount_path, VFS &vfs)
    : _vind(vind), _mount_path(mount_path), _vfs(&vfs) {}

VFile::~VFile() {
    delete _vind.get();
}

void VFile::destruct() {
    if (_vfs != nullptr) {
        _vfs->_on_vfile_destroy(_mount_path);
        _vfs = nullptr;
    }
    delete this;
}

void VFS::init() {
    // call the constructor explicitly to ensure the instance is initialized before use
    new (&inst_vfs) VFS();
    inst_vfs_initialized = true;
}

bool VFS::initialized() {
    return inst_vfs_initialized;
}

VFS &VFS::inst() {
    if (!initialized()) {
        panic("VFS 未初始化!");
    }
    return inst_vfs;
}

Result<IDirectory *> IINode::as_directory() {
    IDirectory *dir = this->as<IDirectory>();
    if (dir) {
        return dir;
    } else {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
}

Result<IFile *> IINode::as_file() {
    IFile *file = this->as<IFile>();
    if (file) {
        return file;
    } else {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
}

VFS::~VFS() = default;

Result<void> VFS::register_fs(util::owner<IFsDriver *> &&driver) {
    const char *fs_name = driver->name();
    if (fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    fs_table.insert_or_assign(fs_name,
                              util::owner(new VFsDriver(std::move(driver))));
    return {};
}

Result<void> VFS::unregister_fs(const char *fs_name) {
    if (!fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto get_res = fs_table.at_nt(fs_name);
    if (!get_res.has_value()) {
        unexpect_return(ErrCode::UNKNOWN_ERROR);
    }

    util::owner<VFsDriver *> driver = get_res.value().get();
    if (!driver->closable()) {
        unexpect_return(ErrCode::BUSY);
    }

    fs_table.erase(fs_name);
    delete driver;
    void_return();
}

Result<void> VFS::mount(const char *fs_name, IBlockDevice *device,
                        const char *mountpoint, MountFlags flags,
                        const char *options) {
    util::Path mnt_path = util::Path::normalize(mountpoint);
    if (mount_table.contains(mnt_path)) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 已经被挂载
    }
    // 否则, 挂载该文件系统
    // 查找文件系统驱动
    auto lookup_result = fs_table.at_nt(fs_name);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 未注册该文件系统
    }
    // 获取文件系统驱动
    VFsDriver *fsd = lookup_result.value().get();

    // 挂载文件系统
    auto mount_result = fsd->fsd()->mount(device, options);
    return mount_result.transform(
        [this, mnt_path, fsd](util::owner<ISuperblock *> isb) {
            util::owner<VSuperblock *> vsb =
                util::owner(new VSuperblock(isb, *fsd));
            this->mount_table.insert_or_assign(mnt_path, vsb);
            this->_active_mount_files.insert_or_assign(mnt_path, 0);
        });
}

Result<void> VFS::umount(const char *mountpoint) {
    util::Path mnt_path = util::Path::normalize(mountpoint);
    auto lookup_result  = mount_table.at_nt(mnt_path);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 没有该挂载点
    }

    auto active_res = _active_mount_files.at_nt(mnt_path);
    if (!active_res.has_value()) {
        unexpect_return(ErrCode::FS_ERROR);
    }
    if (active_res.value().get() != 0) {
        unexpect_return(ErrCode::BUSY);
    }

    util::owner<VSuperblock *> vsb = lookup_result.value().get();

    // 移除挂载
    this->mount_table.erase(mnt_path);
    this->_active_mount_files.erase(mnt_path);
    Result<void> ret = vsb->vfsd().fsd()->unmount(vsb->sb());
    delete vsb;

    return ret;
}

Result<VFile *> VFS::_open_file(const char *filepath) {
    if (filepath == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }
    if (*filepath == '\0') {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    util::Path mount_path;
    auto vind_res = _resolve_inode(util::Path::from(filepath).normalize(),
                                   mount_path);
    propagate(vind_res);

    auto *file = new VFile(vind_res.value(), mount_path, *this);
    if (file == nullptr) {
        delete vind_res.value().get();
        unexpect_return(ErrCode::ALLOCATION_FAILED);
    }

    auto active_res = _active_mount_files.at_nt(mount_path);
    if (!active_res.has_value()) {
        delete file;
        unexpect_return(ErrCode::FS_ERROR);
    }
    active_res.value().get()++;
    return file;
}

Result<CapIdx> VFS::open(const char *filepath, cap::CHolder &holder) {
    auto file_res = _open_file(filepath);
    propagate(file_res);

    auto insert_res = holder.insert_to_free(file_res.value());
    if (!insert_res.has_value()) {
        file_res.value()->destruct();
        propagate_return(insert_res);
    }
    return insert_res.value();
}

Result<VFile *> VFS::__debug_open(const char *filepath) {
    return _open_file(filepath);
}

Result<std::pair<util::Path, VSuperblock *>> VFS::_resolve_mount(
    const util::Path &path) {
    util::Path cur_path = path;
    while (true) {
        auto mount_res = mount_table.at_nt(cur_path);
        if (mount_res.has_value()) {
            return std::make_pair(cur_path, mount_res.value().get().get());
        }
        if (cur_path == "/") {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        cur_path = cur_path.parent_path();
    }
}

Result<util::owner<VINode *>> VFS::_resolve_inode(const util::Path &path,
                                                  util::Path &mount_path) {
    auto mount_res = _resolve_mount(path);
    propagate(mount_res);
    mount_path        = mount_res.value().first;
    VSuperblock *vsb = mount_res.value().second;

    auto root_res = vsb->sb()->root();
    propagate(root_res);

    auto inode_res = vsb->sb()->get_inode(root_res.value());
    propagate(inode_res);

    auto current_vind =
        util::owner(new VINode(inode_res.value(), vsb->vfsd(), *vsb));
    if (!current_vind) {
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }

    const util::Path relpath = path.relative_to(mount_path);
    if (relpath == ".") {
        return current_vind;
    }

    for (const auto &entry : relpath) {
        auto lookup_res = current_vind->inode()->as_directory().and_then(
            [entry](IDirectory *dir) { return dir->lookup(entry); });
        propagate(lookup_res);

        auto next_inode_res = vsb->sb()->get_inode(lookup_res.value());
        propagate(next_inode_res);

        auto next_vind =
            util::owner(new VINode(next_inode_res.value(), vsb->vfsd(), *vsb));
        if (!next_vind) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        current_vind = next_vind;
    }

    return current_vind;
}

void VFS::_on_vfile_destroy(const util::Path &mount_path) noexcept {
    auto active_res = _active_mount_files.at_nt(mount_path);
    if (!active_res.has_value()) {
        loggers::VFS::WARN("VFile 销毁时找不到挂载点: %s",
                           mount_path.c_str());
        return;
    }
    if (active_res.value().get() == 0) {
        loggers::VFS::WARN("VFile 活跃计数已经为 0: %s", mount_path.c_str());
        return;
    }
    active_res.value().get()--;
}

// 读取文件内容到buf中, 返回实际读取的字节数
Result<size_t> VFS::read(VFile &vfile, off_t offset, void *buf, size_t len) const {
    // get interfaces from vfile
    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    IFile *file = file_res.value();

    return file->read(offset, buf, len);
}

// 将buf中的内容写入文件, 返回实际写入的字节数
Result<size_t> VFS::write(VFile &vfile, off_t offset, const void *buf,
                          size_t len) const
{
    // get interfaces from vfile
    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    IFile *file = file_res.value();

    return file->write(offset, buf, len);
}

// 获取文件大小
Result<size_t> VFS::size(VFile &vfile) const
{
    // get interfaces from vfile
    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    IFile *file = file_res.value();

    return file->size();
}

// 刷新文件内容到存储设备
Result<void> VFS::sync(VFile &vfile) const
{
    // get interfaces from vfile
    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    IFile *file = file_res.value();

    return file->sync();
}
