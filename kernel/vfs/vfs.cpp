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

#include <sus/owner.h>
#include <sus/path.h>
#include <sus/raii.h>
#include <sustcore/errcode.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cstring>
#include <expected>

Result<void> VFS::register_fs(util::owner<IFsDriver *> &&driver) {
    const char *fs_name = driver->name();
    if (fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    fs_table.put(fs_name, driver);
    return {};
}

Result<void> VFS::unregister_fs(const char *fs_name) {
    if (!fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    // 遍历superblock表, 确认没有挂载该文件系统
    for (auto &mnt_entry : mount_table) {
        ISuperblock *sb = mnt_entry.second;
        if (strcmp(sb->fs()->name(), fs_name) == 0) {
            unexpect_return(ErrCode::BUSY);
        }
    }
    fs_table.remove(fs_name);
    if (auto driver = fs_table.get(fs_name)) {
        delete driver.value();
    }
    return {};
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
    auto lookup_result = fs_table.get(fs_name);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 未注册该文件系统
    }
    // 获取文件系统驱动
    IFsDriver *fs_driver = lookup_result.value();

    // 挂载文件系统
    auto mount_result = fs_driver->mount(device, options);
    return mount_result.transform([this, mnt_path](ISuperblock *sb) {
        mount_table.put(mnt_path, util::owner(sb));
    });
}

Result<void> VFS::umount(const char *mountpoint) {
    util::Path mnt_path = util::Path::normalize(mountpoint);
    auto lookup_result  = mount_table.get_entry(mnt_path);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 没有该挂载点
    }

    ISuperblock *sb = lookup_result.value().second;

    // 确定没有打开的文件属于该挂载点
    // 先进行一次tidy_up, 清理掉已亡文件
    // 因为我们并不会在文件的引用计数归零时即刻对其进行删除
    Result<void> res = tidy_up();
    if (! res.has_value()) {
        return res;
    }

    for (const auto &open_entry : openlist) {
        if (open_entry.second->superblock() == sb) {
            unexpect_return(ErrCode::BUSY);
        }
    }

    // 移除挂载
    Result<void> ret = sb->fs()->unmount(sb);
    return ret.transform([this, mnt_path]() { mount_table.remove(mnt_path); });
}

Result<util::owner<VFileAccessor *>> VFS::open(const char *filepath) {
    util::Path path = util::Path::from(filepath).normalize();
    // 如果存在于缓存中
    if (openlist.contains(path)) {
        return openlist.get(path)
            .transform_error([](bool error) { return ErrCode::FS_ERROR; })
            .transform([](util::owner<VFile *> file_owner) {
                return util::owner(new VFileAccessor(file_owner.get()));
            });
    }
    // 否则, 尝试打开文件
    // 遍历挂载点, 找到匹配的挂载点
    // 我们希望找到最长的匹配前缀
    const util::Path *best_match = nullptr;
    ISuperblock *best_sb         = nullptr;
    size_t best_match_len        = 0;
    for (auto &mnt_entry : mount_table) {
        util::Path &mnt_path = mnt_entry.first;
        if (path.starts_with(mnt_path)) {
            if (mnt_path.length() > best_match_len) {
                best_match     = &mnt_path;
                best_sb        = mnt_entry.second;
                best_match_len = mnt_path.length();
            }
        }
    }
    if (best_match_len == 0) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 没有匹配的挂载点
    }
    assert(best_sb != nullptr);
    assert(best_match != nullptr);
    // 找到匹配的挂载点, 计算相对路径并打开文件
    auto root_res = best_sb->root();
    if (!root_res.has_value()) {
        unexpect_return(root_res.error());
    }
    IINode *cur_inode  = root_res.value();
    util::Path relpath = path.relative_to(*best_match);
    for (const auto &entry : relpath) {
        auto next_res =
            cur_inode->as_directory()
                .and_then([entry](IDirectory *dir) {
                    return dir->lookup(std::string(entry).c_str());
                })
                .and_then([](IDentry *dentry) { return dentry->inode(); });
        if (!next_res.has_value()) {
            unexpect_return(next_res.error());
        }
        cur_inode = next_res.value();
    }

    auto asfile_res = cur_inode->as_file();
    if (!asfile_res.has_value()) {
        unexpect_return(asfile_res.error());
    }
    IFile *ifile = asfile_res.value();

    // create vfile and append it into openlist
    util::owner<VFile *> vfile_owner = util::owner(new VFile(best_sb->fs(), best_sb, ifile));
    if (!vfile_owner) {
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }
    openlist.put(path, vfile_owner);

    return util::owner(new VFileAccessor(vfile_owner.get()));
}

Result<void> VFS::tidy_up()
{
    // 遍历openlist并记录可以删去的文件
    util::ArrayList<util::Path> closable_list;
    for (const auto &entries : openlist) {
        if (entries.second->closable()) {
            closable_list.push_back(entries.first);
        }
    }

    for (const auto &closable_path : closable_list) {
        auto get_res = openlist.get(closable_path);
        openlist.remove(closable_path);
        if (get_res.has_value()) {
            delete get_res.value();
        }
        else {
            unexpect_return(ErrCode::FS_ERROR);
        }
    }

    void_return();
}