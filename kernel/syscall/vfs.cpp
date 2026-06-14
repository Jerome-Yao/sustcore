/**
 * @file vfs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS 相关系统调用
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <object/vdir.h>
#include <object/vfile.h>
#include <syscall/syscall.h>
#include <syscall/vfs.h>
#include <task/scheduler.h>
#include <vfs/vfs.h>

namespace syscall {
    namespace {
        [[nodiscard]]
        Result<size_t> encode_dir_entry(const DirectoryEntryInfo &entry,
                                        size_t startpos, void *buf,
                                        size_t buflen) {
            // 首先预估该项大小
            size_t record_size =
                sizeof(dir_entry_header) + entry.name.size() + 1;
            if ((startpos + record_size) >= buflen) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            char *_buf = static_cast<char *>(buf);
            auto *header =
                reinterpret_cast<dir_entry_header *>(_buf + startpos);
            header->next_offset = 0;
            header->is_file     = entry.is_file;
            memcpy(_buf + startpos + sizeof(dir_entry_header),
                   entry.name.c_str(), entry.name.size() + 1);
            return record_size;
        }

        [[nodiscard]]
        Result<size_t> encode_dir_entries_to_buffer(
            const std::vector<DirectoryEntryInfo> &entries, void *buf,
            size_t buflen, size_t offset) {
            if (buf == nullptr && buflen != 0) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (offset >= entries.size() ||
                buflen < sizeof(dir_entry_header) + 1)
            {
                return 0;
            }

            size_t pos = 0;
            char *_buf = static_cast<char *>(buf);

            for (size_t i = offset; i < entries.size(); i++) {
                auto encode_res =
                    encode_dir_entry(entries[i], pos, buf, buflen);
                propagate(encode_res);
                size_t record_size = encode_res.value();
                if (pos + record_size >= buflen) {
                    break;
                }
                auto *header =
                    reinterpret_cast<dir_entry_header *>(_buf + pos);
                // 设置下一项的偏移
                header->next_offset = record_size;
                pos += record_size;
            }

            return pos;
        }

        [[nodiscard]]
        Result<cap::CHolder *> current_holder_for_vfs() {
            auto *current = schd::Scheduler::inst().current_tcb();
            if (current == nullptr || current->task == nullptr ||
                current->task->cholder == nullptr)
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return current->task->cholder;
        }

        [[nodiscard]]
        Result<cap::Capability *> lookup_current_cap(CapIdx idx) {
            auto holder_res = current_holder_for_vfs();
            propagate(holder_res);
            return holder_res.value()->lookup(idx);
        }
    }  // namespace

    Result<CapIdx> vfs_open(CapIdx parent_dir_cap, const UString &relpath,
                            flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().open(*parent_res.value(), relpath.kbuf(), oflags,
                                *holder_res.value());
    }

    Result<CapIdx> vfs_opendir(CapIdx parent_dir_cap, const UString &relpath,
                               flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().opendir(*parent_res.value(), relpath.kbuf(), oflags,
                                   *holder_res.value());
    }

    Result<CapIdx> vfs_mkfile(CapIdx parent_dir_cap, const UString &relpath,
                              flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().mkfile(*parent_res.value(), relpath.kbuf(), oflags,
                                  *holder_res.value());
    }

    Result<CapIdx> vfs_mkdir(CapIdx parent_dir_cap, const UString &relpath,
                             flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().mkdir(*parent_res.value(), relpath.kbuf(), oflags,
                                 *holder_res.value());
    }

    Result<size_t> vfs_read(CapIdx file_cap, size_t offset, UBuffer &&buf,
                            size_t len) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::VFileObject obj(util::nnullforce(cap_res.value()));
        auto read_res = obj.read(offset, buf.kbuf(), len);
        propagate(read_res);
        auto commit_res = buf.commit_to_user();
        propagate(commit_res);
        return read_res.value();
    }

    Result<size_t> vfs_write(CapIdx file_cap, size_t offset, UBuffer &&buf,
                             size_t len) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::VFileObject obj(util::nnullforce(cap_res.value()));
        return obj.write(offset, buf.kbuf(), len);
    }

    Result<size_t> vfs_size(CapIdx file_cap) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::VFileObject obj(util::nnullforce(cap_res.value()));
        return obj.size();
    }

    Result<size_t> vfs_getdents(CapIdx dir_cap, UBuffer &&buf, size_t buflen,
                                size_t offset) {
        auto cap_res = lookup_current_cap(dir_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VDIR) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        cap::VDirectoryObject obj(util::nnullforce(cap_res.value()));
        auto entries_res = obj.getdents();
        propagate(entries_res);

        auto dents_res = encode_dir_entries_to_buffer(
            entries_res.value(), buf.kbuf(), buflen, offset);
        propagate(dents_res);

        auto commit_res = buf.commit_to_user();
        propagate(commit_res);
        
        return dents_res.value();
    }

    Result<bool> vfs_sync(CapIdx capidx) {
        auto cap_res = lookup_current_cap(capidx);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() == PayloadType::VFILE) {
            cap::VFileObject obj(util::nnullforce(cap_res.value()));
            auto sync_res = obj.sync();
            propagate(sync_res);
            return true;
        }
        if (cap_res.value()->payload()->type_id() == PayloadType::VDIR) {
            cap::VDirectoryObject obj(util::nnullforce(cap_res.value()));
            auto sync_res = obj.sync();
            propagate(sync_res);
            return true;
        }
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

}  // namespace syscall
