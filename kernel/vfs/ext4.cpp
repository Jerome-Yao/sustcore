/**
 * @file ext4.cpp
 * @author Codex
 * @brief Ext4 block filesystem implementation
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <bio/blk.h>
#include <logger.h>
#include <task/wait.h>
#include <vfs/ext4.h>

#include <algorithm>
#include <cstring>

namespace ext4 {
    namespace {
        constexpr uint16_t EXT4_SUPER_MAGIC = 0xEF53;
        constexpr uint16_t EXT4_EXT_MAGIC   = 0xF30A;
        constexpr inode_t EXT4_ROOT_INO     = 2;

        constexpr uint16_t EXT4_S_IFMT  = 0xF000;
        constexpr uint16_t EXT4_S_IFREG = 0x8000;
        constexpr uint16_t EXT4_S_IFDIR = 0x4000;
        constexpr uint16_t EXT4_S_IFLNK = 0xA000;

        constexpr uint32_t EXT4_EXTENTS_FL = 0x00080000;
        constexpr uint16_t EXT4_LINK_MAX   = 65000;
        constexpr uint16_t EXT4_MAX_EXTENT_DEPTH = 5;

        constexpr uint32_t EXT4_FEATURE_INCOMPAT_FILETYPE = 0x0002;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_EXTENTS  = 0x0040;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT    = 0x0080;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_FLEX_BG  = 0x0200;
        constexpr uint32_t EXT4_SUPPORTED_INCOMPAT =
            EXT4_FEATURE_INCOMPAT_FILETYPE | EXT4_FEATURE_INCOMPAT_EXTENTS |
            EXT4_FEATURE_INCOMPAT_64BIT | EXT4_FEATURE_INCOMPAT_FLEX_BG;

        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER = 0x0001;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_LARGE_FILE   = 0x0002;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_BTREE_DIR    = 0x0004;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_HUGE_FILE    = 0x0008;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_GDT_CSUM     = 0x0010;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_DIR_NLINK    = 0x0020;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE  = 0x0040;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_BIGALLOC     = 0x0200;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_METADATA_CSUM = 0x0400;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_READONLY     = 0x1000;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_PROJECT      = 0x2000;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_VERITY       = 0x8000;
        constexpr uint32_t EXT4_SUPPORTED_RO_COMPAT =
            EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER |
            EXT4_FEATURE_RO_COMPAT_LARGE_FILE |
            EXT4_FEATURE_RO_COMPAT_BTREE_DIR |
            EXT4_FEATURE_RO_COMPAT_HUGE_FILE |
            EXT4_FEATURE_RO_COMPAT_GDT_CSUM |
            EXT4_FEATURE_RO_COMPAT_DIR_NLINK |
            EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE |
            EXT4_FEATURE_RO_COMPAT_METADATA_CSUM |
            EXT4_FEATURE_RO_COMPAT_READONLY |
            EXT4_FEATURE_RO_COMPAT_PROJECT |
            EXT4_FEATURE_RO_COMPAT_VERITY;

        constexpr uint8_t EXT4_FT_UNKNOWN  = 0;
        constexpr uint8_t EXT4_FT_REG_FILE = 1;
        constexpr uint8_t EXT4_FT_DIR      = 2;
        constexpr uint8_t EXT4_FT_SYMLINK  = 7;

        template <typename T>
        [[nodiscard]]
        T read_le(const void *ptr) {
            T value {};
            memcpy(&value, ptr, sizeof(T));
            return value;
        }

        template <typename T>
        [[nodiscard]]
        T read_le_at(const std::vector<byte> &buf, size_t offset) {
            if (offset + sizeof(T) > buf.size()) {
                return T {};
            }
            return read_le<T>(buf.data() + offset);
        }

        [[nodiscard]]
        uint64_t join_u64(uint32_t lo, uint32_t hi) {
            return static_cast<uint64_t>(lo) |
                   (static_cast<uint64_t>(hi) << 32);
        }

        struct PACKED Ext4ExtentHeader {
            uint16_t eh_magic;
            uint16_t eh_entries;
            uint16_t eh_max;
            uint16_t eh_depth;
            uint32_t eh_generation;
        };

        struct PACKED Ext4Extent {
            uint32_t ee_block;
            uint16_t ee_len;
            uint16_t ee_start_hi;
            uint32_t ee_start_lo;
        };

        struct PACKED Ext4ExtentIdx {
            uint32_t ei_block;
            uint32_t ei_leaf_lo;
            uint16_t ei_leaf_hi;
            uint16_t ei_unused;
        };

        struct PACKED Ext4DirEntry2 {
            uint32_t inode;
            uint16_t rec_len;
            uint8_t name_len;
            uint8_t file_type;
        };

        [[nodiscard]]
        bool valid_inode_id(inode_t inode_id) {
            return inode_id != 0;
        }

        [[nodiscard]]
        uint32_t extent_len(uint16_t len_raw) {
            if (len_raw > 0x8000U) {
                return static_cast<uint32_t>(len_raw - 0x8000U);
            }
            return len_raw;
        }

        [[nodiscard]]
        bool extent_unwritten(uint16_t len_raw) {
            return len_raw > 0x8000U;
        }
    }  // namespace

    Ext4File::Ext4File(Ext4Superblock &sb, inode_t inode_id) noexcept
        : _sb(&sb), _inode_id(inode_id) {}

    Result<size_t> Ext4File::read(off_t offset, void *buf, size_t len) {
        if (offset < 0 || (len != 0 && buf == nullptr)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return _sb->read_inode_data(_inode_id, static_cast<uint64_t>(offset),
                                    buf, len);
    }

    Result<size_t> Ext4File::write(off_t offset, const void *buf, size_t len) {
        if (offset < 0 || (len != 0 && buf == nullptr)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return _sb->write_inode_data(_inode_id, static_cast<uint64_t>(offset),
                                     buf, len);
    }

    Result<size_t> Ext4File::size() {
        auto size_res = _sb->inode_size(_inode_id);
        propagate(size_res);
        return static_cast<size_t>(size_res.value());
    }

    Result<void> Ext4File::sync() {
        return _sb->sync();
    }

    IMetadata &Ext4File::metadata() {
        return _metadata;
    }

    inode_t Ext4File::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy Ext4File::inode_cache() const {
        return INodeCachePolicy::SHARED;
    }

    FileCachePolicy Ext4File::file_cache() const {
        return FileCachePolicy::SHARED;
    }

    Ext4Directory::Ext4Directory(Ext4Superblock &sb,
                                 inode_t inode_id) noexcept
        : _sb(&sb), _inode_id(inode_id) {}

    Result<std::vector<Ext4DirEntry>> Ext4Directory::collect_entries() {
        return _sb->read_directory(_inode_id);
    }

    Result<inode_t> Ext4Directory::lookup(std::string_view name) {
        if (name.empty() || name == ".") {
            return _inode_id;
        }

        auto entries_res = collect_entries();
        propagate(entries_res);
        for (const auto &entry : entries_res.value()) {
            if (entry.name == name) {
                return entry.inode_id;
            }
        }
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<inode_t> Ext4Directory::mkfile(std::string_view name,
                                          const char *options) {
        (void)name;
        (void)options;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<inode_t> Ext4Directory::mkdir(std::string_view name,
                                         const char *options) {
        (void)name;
        (void)options;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<size_t> Ext4Directory::entry_count() {
        auto entries_res = collect_entries();
        propagate(entries_res);
        return entries_res.value().size();
    }

    Result<DirectoryEntryInfo> Ext4Directory::entry_at(size_t index) {
        auto entries_res = collect_entries();
        propagate(entries_res);
        if (index >= entries_res.value().size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const auto &entry = entries_res.value()[index];
        return DirectoryEntryInfo{
            .is_file = entry.is_file,
            .name    = entry.name,
        };
    }

    Result<void> Ext4Directory::sync() {
        return _sb->sync();
    }

    IMetadata &Ext4Directory::metadata() {
        return _metadata;
    }

    inode_t Ext4Directory::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy Ext4Directory::inode_cache() const {
        return INodeCachePolicy::SHARED;
    }

    Ext4Superblock::Ext4Superblock(Ext4Driver &fs, blk::BufferCache &cache,
                                   size_t dev_block_size,
                                   size_t dev_block_count, size_t sb_id)
        : _fs(&fs),
          _cache(&cache),
          _dev_block_size(dev_block_size),
          _dev_block_count(dev_block_count),
          _sb_id(sb_id) {}

    Result<void> Ext4Superblock::read_device_bytes(uint64_t offset, void *buf,
                                                   size_t len) {
        if (len == 0) {
            void_return();
        }
        if (buf == nullptr || _cache == nullptr || _dev_block_size == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint64_t total_bytes =
            static_cast<uint64_t>(_dev_block_size) * _dev_block_count;
        if (offset > total_bytes || len > total_bytes - offset) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        auto *out = static_cast<byte *>(buf);
        size_t done = 0;
        while (done < len) {
            const uint64_t abs = offset + done;
            const lba_t lba    = static_cast<lba_t>(abs / _dev_block_size);
            const size_t off   = static_cast<size_t>(abs % _dev_block_size);
            const size_t chunk = std::min(len - done, _dev_block_size - off);

            auto future      = _cache->get_buffer_async(lba);
            auto handler_res = wait::kthread_wait_for(future);
            propagate(handler_res);
            const size_t read = handler_res.value().read(off, out + done,
                                                         chunk);
            if (read != chunk) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            done += chunk;
        }
        void_return();
    }

    Result<void> Ext4Superblock::write_device_bytes(uint64_t offset,
                                                    const void *buf,
                                                    size_t len) {
        if (len == 0) {
            void_return();
        }
        if (buf == nullptr || _cache == nullptr || _dev_block_size == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint64_t total_bytes =
            static_cast<uint64_t>(_dev_block_size) * _dev_block_count;
        if (offset > total_bytes || len > total_bytes - offset) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        const auto *in = static_cast<const byte *>(buf);
        size_t done    = 0;
        while (done < len) {
            const uint64_t abs = offset + done;
            const lba_t lba    = static_cast<lba_t>(abs / _dev_block_size);
            const size_t off   = static_cast<size_t>(abs % _dev_block_size);
            const size_t chunk = std::min(len - done, _dev_block_size - off);

            auto future      = _cache->get_buffer_async(lba);
            auto handler_res = wait::kthread_wait_for(future);
            propagate(handler_res);
            const size_t written =
                handler_res.value().write(off, in + done, chunk);
            if (written != chunk) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            done += chunk;
        }
        void_return();
    }

    Result<void> Ext4Superblock::read_fs_block(uint64_t block, void *buf,
                                               size_t len) {
        if (len > _block_size) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return read_device_bytes(block * _block_size, buf, len);
    }

    Result<void> Ext4Superblock::write_fs_block(uint64_t block,
                                                const void *buf, size_t len) {
        if (len > _block_size) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return write_device_bytes(block * _block_size, buf, len);
    }

    Result<void> Ext4Superblock::load() {
        _superblock.resize(1024);
        auto super_res = read_device_bytes(1024, _superblock.data(),
                                           _superblock.size());
        propagate(super_res);

        const auto magic = read_le_at<uint16_t>(_superblock, 56);
        if (magic != EXT4_SUPER_MAGIC) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const uint32_t log_block_size = read_le_at<uint32_t>(_superblock, 24);
        if (log_block_size > 16) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        _block_size = 1024U << log_block_size;
        if (_block_size == 0 || (_block_size % _dev_block_size) != 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        _first_data_block  = read_le_at<uint32_t>(_superblock, 20);
        _blocks_per_group  = read_le_at<uint32_t>(_superblock, 32);
        _inodes_per_group  = read_le_at<uint32_t>(_superblock, 40);
        _inode_size        = read_le_at<uint16_t>(_superblock, 88);
        _feature_compat    = read_le_at<uint32_t>(_superblock, 92);
        _feature_incompat  = read_le_at<uint32_t>(_superblock, 96);
        _feature_ro_compat = read_le_at<uint32_t>(_superblock, 100);
        _group_desc_size   = read_le_at<uint16_t>(_superblock, 254);
        if (_inode_size == 0) {
            _inode_size = 128;
        }
        if (_group_desc_size == 0) {
            _group_desc_size = 32;
        }

        if ((_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) == 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        if ((_feature_incompat & ~EXT4_SUPPORTED_INCOMPAT) != 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        if ((_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_BIGALLOC) != 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        const uint32_t unsupported_ro =
            _feature_ro_compat & ~EXT4_SUPPORTED_RO_COMPAT;
        _read_only =
            unsupported_ro != 0 ||
            (_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_READONLY) != 0;
        if (_blocks_per_group == 0 || _inodes_per_group == 0 ||
            _inode_size < 128 || _group_desc_size < 32)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const uint32_t blocks_lo = read_le_at<uint32_t>(_superblock, 4);
        const uint32_t blocks_hi = read_le_at<uint32_t>(_superblock, 336);
        _block_count = join_u64(blocks_lo, blocks_hi);
        if (_block_count == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        _group_count = static_cast<uint32_t>(
            (_block_count - _first_data_block + _blocks_per_group - 1) /
            _blocks_per_group);

        const uint64_t desc_table_block =
            _block_size == 1024 ? 2 : _first_data_block + 1;
        const size_t desc_bytes =
            static_cast<size_t>(_group_count) * _group_desc_size;
        _group_desc.resize(desc_bytes);
        auto gd_res = read_device_bytes(desc_table_block * _block_size,
                                        _group_desc.data(),
                                        _group_desc.size());
        propagate(gd_res);

        loggers::VFS::INFO(
            "Ext4 挂载: block_size=%u groups=%u inode_size=%u desc_size=%u features(incompat)=0x%x ro=0x%x compat=0x%x",
            static_cast<unsigned>(_block_size),
            static_cast<unsigned>(_group_count),
            static_cast<unsigned>(_inode_size),
            static_cast<unsigned>(_group_desc_size),
            static_cast<unsigned>(_feature_incompat),
            static_cast<unsigned>(_feature_ro_compat),
            static_cast<unsigned>(_feature_compat));
        if (_read_only) {
            loggers::VFS::WARN(
                "Ext4 以只读模式挂载: ro_compat=0x%x unsupported_ro=0x%x",
                static_cast<unsigned>(_feature_ro_compat),
                static_cast<unsigned>(unsupported_ro));
        }
        void_return();
    }

    Result<uint64_t> Ext4Superblock::inode_table_block(uint32_t group) {
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 44 > _group_desc.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const uint32_t lo = read_le<uint32_t>(_group_desc.data() + base + 8);
        uint32_t hi       = 0;
        if (_group_desc_size >= 64) {
            hi = read_le<uint32_t>(_group_desc.data() + base + 40);
        }
        return join_u64(lo, hi);
    }

    Result<std::vector<byte>> Ext4Superblock::read_inode_raw(
        inode_t inode_id) {
        if (!valid_inode_id(inode_id)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const inode_t zero_based = inode_id - 1;
        const uint32_t group =
            static_cast<uint32_t>(zero_based / _inodes_per_group);
        const uint32_t index =
            static_cast<uint32_t>(zero_based % _inodes_per_group);
        auto table_res = inode_table_block(group);
        propagate(table_res);

        std::vector<byte> raw(_inode_size);
        const uint64_t inode_offset =
            table_res.value() * _block_size +
            static_cast<uint64_t>(index) * _inode_size;
        auto read_res = read_device_bytes(inode_offset, raw.data(), raw.size());
        propagate(read_res);
        return raw;
    }

    Result<void> Ext4Superblock::validate_inode_raw(
        const std::vector<byte> &raw) {
        if (raw.size() < 128) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint16_t mode        = read_le_at<uint16_t>(raw, 0);
        const uint32_t delete_time = read_le_at<uint32_t>(raw, 20);
        const uint16_t link_count  = read_le_at<uint16_t>(raw, 26);
        if (mode == 0 || delete_time != 0 || link_count == 0) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (link_count > EXT4_LINK_MAX &&
            (_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_DIR_NLINK) == 0)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    Result<uint16_t> Ext4Superblock::inode_mode(inode_t inode_id) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        return read_le_at<uint16_t>(raw_res.value(), 0);
    }

    Result<uint64_t> Ext4Superblock::inode_size(inode_t inode_id) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        const uint16_t mode = read_le_at<uint16_t>(raw_res.value(), 0);
        const uint64_t lo   = read_le_at<uint32_t>(raw_res.value(), 4);
        uint64_t hi         = 0;
        if ((mode & EXT4_S_IFMT) == EXT4_S_IFREG ||
            (mode & EXT4_S_IFMT) == EXT4_S_IFLNK)
        {
            hi = read_le_at<uint32_t>(raw_res.value(), 108);
        }
        return lo | (hi << 32);
    }

    Result<Ext4ExtentMapping> Ext4Superblock::extent_lookup_from_node(
        const byte *node, size_t node_size, uint32_t logical) {
        if (node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (node_size < sizeof(Ext4ExtentHeader)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *header = reinterpret_cast<const Ext4ExtentHeader *>(node);
        if (read_le<uint16_t>(&header->eh_magic) != EXT4_EXT_MAGIC) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint16_t entries = read_le<uint16_t>(&header->eh_entries);
        const uint16_t max     = read_le<uint16_t>(&header->eh_max);
        const uint16_t depth   = read_le<uint16_t>(&header->eh_depth);
        if (entries > max) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (depth > EXT4_MAX_EXTENT_DEPTH) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (entries == 0) {
            return Ext4ExtentMapping {};
        }

        if (depth == 0) {
            const size_t entries_bytes =
                static_cast<size_t>(entries) * sizeof(Ext4Extent);
            if (sizeof(Ext4ExtentHeader) + entries_bytes > node_size) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            auto *extents = reinterpret_cast<const Ext4Extent *>(
                node + sizeof(Ext4ExtentHeader));
            for (uint16_t i = 0; i < entries; ++i) {
                const uint32_t first = read_le<uint32_t>(&extents[i].ee_block);
                const uint16_t len_raw =
                    read_le<uint16_t>(&extents[i].ee_len);
                const uint32_t len = extent_len(len_raw);
                if (len == 0) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                if (logical < first || logical >= first + len) {
                    continue;
                }
                const uint64_t start =
                    (static_cast<uint64_t>(
                         read_le<uint16_t>(&extents[i].ee_start_hi))
                     << 32) |
                    read_le<uint32_t>(&extents[i].ee_start_lo);
                const uint64_t physical = start + (logical - first);
                if (physical >= _block_count) {
                    unexpect_return(ErrCode::OUT_OF_BOUNDARY);
                }
                return Ext4ExtentMapping{
                    .physical_block = physical,
                    .mapped         = true,
                    .unwritten      = extent_unwritten(len_raw),
                };
            }
            return Ext4ExtentMapping {};
        }

        const size_t index_bytes =
            static_cast<size_t>(entries) * sizeof(Ext4ExtentIdx);
        if (sizeof(Ext4ExtentHeader) + index_bytes > node_size) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto *indexes = reinterpret_cast<const Ext4ExtentIdx *>(
            node + sizeof(Ext4ExtentHeader));
        const Ext4ExtentIdx *chosen = nullptr;
        for (uint16_t i = 0; i < entries; ++i) {
            const uint32_t first = read_le<uint32_t>(&indexes[i].ei_block);
            if (first > logical) {
                break;
            }
            chosen = &indexes[i];
        }
        if (chosen == nullptr) {
            return Ext4ExtentMapping {};
        }

        const uint64_t leaf =
            (static_cast<uint64_t>(read_le<uint16_t>(&chosen->ei_leaf_hi))
             << 32) |
            read_le<uint32_t>(&chosen->ei_leaf_lo);
        if (leaf >= _block_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        std::vector<byte> child(_block_size);
        auto read_res = read_fs_block(leaf, child.data(), child.size());
        propagate(read_res);
        return extent_lookup_from_node(child.data(), child.size(), logical);
    }

    Result<Ext4ExtentMapping> Ext4Superblock::extent_lookup(inode_t inode_id,
                                                            uint32_t logical) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        if (raw_res.value().size() < 100) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint32_t flags = read_le_at<uint32_t>(raw_res.value(), 32);
        if ((flags & EXT4_EXTENTS_FL) == 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        return extent_lookup_from_node(raw_res.value().data() + 40, 60,
                                       logical);
    }

    Result<bool> Ext4Superblock::dir_entry_is_file(inode_t inode_id,
                                                   uint8_t file_type) {
        if ((_feature_incompat & EXT4_FEATURE_INCOMPAT_FILETYPE) != 0) {
            if (file_type == EXT4_FT_DIR) {
                return false;
            }
            if (file_type == EXT4_FT_REG_FILE ||
                file_type == EXT4_FT_SYMLINK)
            {
                return true;
            }
            if (file_type != EXT4_FT_UNKNOWN) {
                return true;
            }
        }

        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        const uint16_t type = mode_res.value() & EXT4_S_IFMT;
        return type != EXT4_S_IFDIR;
    }

    Result<size_t> Ext4Superblock::read_inode_data(inode_t inode_id,
                                                   uint64_t offset, void *buf,
                                                   size_t len) {
        if (len == 0) {
            return 0;
        }
        if (buf == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);
        if (offset >= size_res.value()) {
            return 0;
        }

        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        const uint16_t mode = read_le_at<uint16_t>(raw_res.value(), 0);
        if ((mode & EXT4_S_IFMT) == EXT4_S_IFLNK && size_res.value() <= 60) {
            const size_t readable = static_cast<size_t>(
                std::min<uint64_t>(len, size_res.value() - offset));
            memcpy(buf, raw_res.value().data() + 40 + offset, readable);
            return readable;
        }

        const size_t readable = static_cast<size_t>(
            std::min<uint64_t>(len, size_res.value() - offset));
        auto *out = static_cast<byte *>(buf);
        size_t done = 0;
        while (done < readable) {
            const uint64_t abs      = offset + done;
            const uint32_t logical  = static_cast<uint32_t>(abs / _block_size);
            const size_t block_off  = static_cast<size_t>(abs % _block_size);
            const size_t chunk =
                std::min(readable - done, static_cast<size_t>(_block_size) -
                                              block_off);
            auto phys_res = extent_lookup(inode_id, logical);
            propagate(phys_res);
            if (!phys_res.value().mapped || phys_res.value().unwritten) {
                memset(out + done, 0, chunk);
            } else {
                auto read_res = read_device_bytes(
                    phys_res.value().physical_block * _block_size + block_off,
                    out + done, chunk);
                propagate(read_res);
            }
            done += chunk;
        }
        return done;
    }

    Result<size_t> Ext4Superblock::write_inode_data(inode_t inode_id,
                                                    uint64_t offset,
                                                    const void *buf,
                                                    size_t len) {
        if (len == 0) {
            return 0;
        }
        if (buf == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        if ((mode_res.value() & EXT4_S_IFMT) != EXT4_S_IFREG) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);
        if (offset >= size_res.value()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        const size_t writable = static_cast<size_t>(
            std::min<uint64_t>(len, size_res.value() - offset));
        const auto *in = static_cast<const byte *>(buf);
        size_t done    = 0;
        while (done < writable) {
            const uint64_t abs      = offset + done;
            const uint32_t logical  = static_cast<uint32_t>(abs / _block_size);
            const size_t block_off  = static_cast<size_t>(abs % _block_size);
            const size_t chunk =
                std::min(writable - done, static_cast<size_t>(_block_size) -
                                               block_off);
            auto phys_res = extent_lookup(inode_id, logical);
            propagate(phys_res);
            if (!phys_res.value().mapped || phys_res.value().unwritten) {
                unexpect_return(ErrCode::NOT_SUPPORTED);
            }
            auto write_res = write_device_bytes(
                phys_res.value().physical_block * _block_size + block_off,
                in + done, chunk);
            propagate(write_res);
            done += chunk;
        }
        return done;
    }

    Result<std::vector<Ext4DirEntry>> Ext4Superblock::read_directory(
        inode_t inode_id) {
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        if ((mode_res.value() & EXT4_S_IFMT) != EXT4_S_IFDIR) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);

        std::vector<Ext4DirEntry> entries {};
        std::vector<byte> block(_block_size);
        const uint64_t block_count =
            (size_res.value() + _block_size - 1) / _block_size;
        for (uint64_t logical = 0; logical < block_count; ++logical) {
            auto phys_res = extent_lookup(inode_id,
                                          static_cast<uint32_t>(logical));
            propagate(phys_res);
            if (!phys_res.value().mapped || phys_res.value().unwritten) {
                continue;
            }
            auto read_res = read_fs_block(phys_res.value().physical_block,
                                          block.data(), block.size());
            propagate(read_res);

            size_t off = 0;
            while (off + sizeof(Ext4DirEntry2) <= block.size()) {
                auto *dirent = reinterpret_cast<const Ext4DirEntry2 *>(
                    block.data() + off);
                const uint32_t ino = read_le<uint32_t>(&dirent->inode);
                const uint16_t rec_len = read_le<uint16_t>(&dirent->rec_len);
                const uint8_t name_len = dirent->name_len;
                const uint8_t file_type = dirent->file_type;
                if (rec_len < sizeof(Ext4DirEntry2) ||
                    off + rec_len > block.size() ||
                    name_len > rec_len - sizeof(Ext4DirEntry2))
                {
                    break;
                }

                if (ino != 0 && name_len != 0) {
                    const char *name = reinterpret_cast<const char *>(
                        block.data() + off + sizeof(Ext4DirEntry2));
                    std::string entry_name(name, name_len);
                    if (entry_name != "." && entry_name != "..") {
                        auto is_file_res = dir_entry_is_file(
                            static_cast<inode_t>(ino), file_type);
                        propagate(is_file_res);
                        entries.push_back(Ext4DirEntry{
                            .inode_id = static_cast<inode_t>(ino),
                            .is_file  = is_file_res.value(),
                            .name     = entry_name,
                        });
                    }
                }
                off += rec_len;
            }
        }
        return entries;
    }

    IFsDriver &Ext4Superblock::fs() {
        return *_fs;
    }

    Result<void> Ext4Superblock::sync() {
        if (_cache == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto future = _cache->sync_all();
        auto res    = wait::kthread_wait_for(future);
        propagate(res);
        void_return();
    }

    Result<inode_t> Ext4Superblock::root() {
        return EXT4_ROOT_INO;
    }

    Result<util::owner<IINode *>> Ext4Superblock::get_inode(inode_t inode_id) {
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        const uint16_t type = mode_res.value() & EXT4_S_IFMT;
        if (type == EXT4_S_IFDIR) {
            auto *dir = new Ext4Directory(*this, inode_id);
            if (dir == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            return util::owner<IINode *>(dir);
        }
        if (type == EXT4_S_IFREG || type == EXT4_S_IFLNK) {
            auto *file = new Ext4File(*this, inode_id);
            if (file == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            return util::owner<IINode *>(file);
        }
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<inode_t> Ext4Superblock::alloc_inode(INodeType type) {
        (void)type;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Ext4Superblock::free_inode(inode_t id) {
        (void)id;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    IMetadata &Ext4Superblock::metadata() {
        return _metadata;
    }

    size_t Ext4Superblock::sb_id() const {
        return _sb_id;
    }

    const char *Ext4Driver::name() const {
        return "ext4";
    }

    Result<void> Ext4Driver::probe(size_t devno, const char *options) {
        auto mount_res = mount(devno, options);
        propagate(mount_res);
        auto unmount_res = unmount(mount_res.value().get());
        propagate(unmount_res);
        delete mount_res.value().get();
        void_return();
    }

    Result<util::owner<ISuperblock *>> Ext4Driver::mount(
        size_t devno, const char *options) {
        (void)options;
        auto device_res = blk::BlkManager::inst().lookup(devno);
        propagate(device_res);
        auto cache_res = blk::BlkManager::inst().lookup_cache(devno);
        propagate(cache_res);
        auto *device = device_res.value();
        auto *cache  = cache_res.value();
        if (device == nullptr || cache == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto block_size_res = device->block_sz();
        propagate(block_size_res);
        auto block_count_res = device->block_cnt();
        propagate(block_count_res);

        auto *sb = new Ext4Superblock(*this, *cache, block_size_res.value(),
                                      block_count_res.value(), _next_sb_id++);
        if (sb == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto load_res = sb->load();
        if (!load_res.has_value()) {
            delete sb;
            propagate_return(load_res);
        }
        return util::owner<ISuperblock *>(sb);
    }

    Result<void> Ext4Driver::unmount(ISuperblock *sb) {
        (void)sb;
        void_return();
    }
}  // namespace ext4
