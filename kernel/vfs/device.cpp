/**
 * @file device.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备文件系统
 * @version alpha-1.0.0
 * @date 2026-06-12
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#include <vfs/device.h>
#include <vfs/vfs.h>

namespace devfs {
    Result<size_t> CharDevFile::read(void *buf, size_t len) {
        (void)buf;
        (void)len;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<size_t> CharDevFile::read(off_t offset, void *buf, size_t len) {
        if (offset != 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return read(buf, len);
    }

    Result<size_t> CharDevFile::write(off_t offset, const void *buf,
                                      size_t len) {
        if (offset != 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return write(buf, len);
    }

    Result<size_t> CharDevFile::size() {
        return 0;
    }

    Result<void> CharDevFile::sync() {
        void_return();
    }

    inode_t CharDevFile::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy CharDevFile::inode_cache() const {
        return INodeCachePolicy::PERMANENT;
    }

    FileCachePolicy CharDevFile::file_cache() const {
        return FileCachePolicy::NONE;
    }

    IMetadata &CharDevFile::metadata() {
        return _metadata;
    }

    DevFSDirectory::DevFSDirectory(DevFSSuperblock &sb, inode_t inode_id) noexcept
        : _sb(&sb), _inode_id(inode_id) {}

    Result<inode_t> DevFSDirectory::lookup(std::string_view name) {
        return _sb->lookup(_inode_id, name);
    }

    Result<inode_t> DevFSDirectory::mkfile(std::string_view name,
                                           const char *options) {
        (void)options;
        return _sb->mkfile(_inode_id, std::string(name));
    }

    Result<inode_t> DevFSDirectory::mkdir(std::string_view name,
                                          const char *options) {
        (void)options;
        return _sb->mkdir(_inode_id, std::string(name));
    }

    Result<size_t> DevFSDirectory::entry_count() {
        auto record_res = _sb->lookup_record(_inode_id);
        propagate(record_res);
        return record_res.value()->entries.size();
    }

    Result<DirectoryEntryInfo> DevFSDirectory::entry_at(size_t index) {
        auto record_res = _sb->lookup_record(_inode_id);
        propagate(record_res);
        auto *record = record_res.value();
        if (index >= record->entries.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        auto it = record->entries.begin();
        for (size_t i = 0; i < index; ++i) {
            ++it;
        }

        auto child_res = _sb->lookup_record(it->second);
        propagate(child_res);
        auto *child = child_res.value();
        return DirectoryEntryInfo{
            .is_file = child->state != DevFSSuperblock::NodeState::DIRECTORY,
            .name    = it->first,
        };
    }

    Result<void> DevFSDirectory::sync() {
        void_return();
    }

    IMetadata &DevFSDirectory::metadata() {
        auto meta_res = _sb->inode_metadata(_inode_id);
        assert(meta_res.has_value());
        return *meta_res.value();
    }

    inode_t DevFSDirectory::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy DevFSDirectory::inode_cache() const {
        return INodeCachePolicy::NONE;
    }

    DevFSUnboundFile::DevFSUnboundFile(inode_t inode_id) noexcept
        : _inode_id(inode_id) {}

    Result<size_t> DevFSUnboundFile::read(off_t offset, void *buf, size_t len) {
        (void)offset;
        (void)buf;
        (void)len;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<size_t> DevFSUnboundFile::write(off_t offset, const void *buf,
                                           size_t len) {
        (void)offset;
        (void)buf;
        (void)len;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<size_t> DevFSUnboundFile::size() {
        return 0;
    }

    Result<void> DevFSUnboundFile::sync() {
        void_return();
    }

    IMetadata &DevFSUnboundFile::metadata() {
        return _metadata;
    }

    inode_t DevFSUnboundFile::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy DevFSUnboundFile::inode_cache() const {
        return INodeCachePolicy::NONE;
    }

    FileCachePolicy DevFSUnboundFile::file_cache() const {
        return FileCachePolicy::NONE;
    }

    DevFSSuperblock::DevFSSuperblock(DevFSDriver *fs, size_t sb_id)
        : _fs(fs), _sb_id(sb_id) {
        _nodes.insert_or_assign(0, NodeRecord{
                                       .inode_id     = 0,
                                       .state        = NodeState::DIRECTORY,
                                       .name         = "/",
                                       .parent_inode = 0,
                                       .entries      = {},
                                   });
    }

    Result<DevFSSuperblock::NodeRecord *> DevFSSuperblock::lookup_record(
        inode_t inode_id) {
        auto record_res = _nodes.at_nt(inode_id);
        if (!record_res.has_value()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return record_res.value();
    }

    Result<const DevFSSuperblock::NodeRecord *>
    DevFSSuperblock::lookup_record(inode_t inode_id) const {
        auto record_res = _nodes.at_nt(inode_id);
        if (!record_res.has_value()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return record_res.value();
    }

    Result<inode_t> DevFSSuperblock::create_node(inode_t parent_inode,
                                                 std::string_view name,
                                                 NodeState state) {
        if (name.empty() || name == "." || name == "..") {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        for (char ch : name) {
            if (ch == '/') {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
        }

        auto parent_res = lookup_record(parent_inode);
        propagate(parent_res);
        auto *parent = parent_res.value();
        if (parent->state != NodeState::DIRECTORY) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (parent->entries.contains(std::string(name))) {
            unexpect_return(ErrCode::KEY_DUPLICATED);
        }

        const inode_t inode_id = _next_inode++;
        parent->entries.insert_or_assign(std::string(name), inode_id);
        _nodes.insert_or_assign(inode_id, NodeRecord{
                                              .inode_id     = inode_id,
                                              .state        = state,
                                              .name         = std::string(name),
                                              .parent_inode = parent_inode,
                                              .entries      = {},
                                          });
        return inode_id;
    }

    IFsDriver &DevFSSuperblock::fs() {
        return *_fs;
    }

    Result<void> DevFSSuperblock::sync() {
        void_return();
    }

    Result<inode_t> DevFSSuperblock::root() {
        return 0;
    }

    Result<util::owner<IINode *>> DevFSSuperblock::get_inode(inode_t inode_id) {
        auto record_res = lookup_record(inode_id);
        propagate(record_res);
        const auto *record = record_res.value();

        switch (record->state) {
            case NodeState::DIRECTORY: {
                auto *dir = new DevFSDirectory(*this, inode_id);
                if (dir == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                return util::owner<IINode *>(dir);
            }
            case NodeState::UNBOUND_FILE: {
                auto *file = new DevFSUnboundFile(inode_id);
                if (file == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                return util::owner<IINode *>(file);
            }
            case NodeState::CHAR_DEVICE:
                if (!record->has_factory || record->char_factory.create == nullptr)
                {
                    unexpect_return(ErrCode::FAILURE);
                }
                return record->char_factory.create(record->char_factory.ctx,
                                                   inode_id);
        }
        unexpect_return(ErrCode::FAILURE);
    }

    IMetadata &DevFSSuperblock::metadata() {
        return _metadata;
    }

    size_t DevFSSuperblock::sb_id() const {
        return _sb_id;
    }

    Result<inode_t> DevFSSuperblock::mkdir(inode_t parent_inode,
                                           std::string name) {
        return create_node(parent_inode, name, NodeState::DIRECTORY);
    }

    Result<inode_t> DevFSSuperblock::mkfile(inode_t parent_inode,
                                            std::string name) {
        return create_node(parent_inode, name, NodeState::UNBOUND_FILE);
    }

    Result<void> DevFSSuperblock::link_char(cap::Capability &filecap,
                                            CharFactory factory) {
        auto *vfile = filecap.payload_as<VFile>();
        if (vfile == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        auto *sb = vfile->vinode()->superblock().sb();
        if (sb != this) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (factory.create == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        const auto inode_id = vfile->vinode()->inode()->inode_id();
        auto record_res = lookup_record(inode_id);
        propagate(record_res);
        auto *record = record_res.value();

        if (record->state != NodeState::UNBOUND_FILE) {
            unexpect_return(ErrCode::BUSY);
        }
        record->state        = NodeState::CHAR_DEVICE;
        record->char_factory = factory;
        record->has_factory  = true;
        void_return();
    }

    Result<inode_t> DevFSSuperblock::alloc_inode(INodeType type) {
        (void)type;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> DevFSSuperblock::free_inode(inode_t id) {
        (void)id;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<inode_t> DevFSSuperblock::lookup(inode_t parent_inode,
                                            std::string_view name) {
        auto parent_res = lookup_record(parent_inode);
        propagate(parent_res);
        auto *parent = parent_res.value();
        if (parent->state != NodeState::DIRECTORY) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (name.empty()) {
            return parent_inode;
        }

        auto it = parent->entries.find(std::string(name));
        if (it == parent->entries.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return it->second;
    }

    Result<IMetadata *> DevFSSuperblock::inode_metadata(inode_t inode_id) {
        auto record_res = lookup_record(inode_id);
        propagate(record_res);
        return &record_res.value()->metadata;
    }

    const char *DevFSDriver::name() const {
        return "devfs";
    }

    Result<void> DevFSDriver::probe(const char *name, const char *options) {
        (void)options;
        if (std::string_view(name) != "devfs") {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        void_return();
    }

    Result<util::owner<ISuperblock *>> DevFSDriver::mount(const char *name,
                                                          const char *options) {
        (void)options;
        if (std::string_view(name) != "devfs") {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        if (_mounted != nullptr) {
            unexpect_return(ErrCode::BUSY);
        }
        auto *sb = new DevFSSuperblock(this, _next_sb_id++);
        if (sb == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        _mounted = sb;
        return util::owner<ISuperblock *>(sb);
    }

    Result<void> DevFSDriver::unmount(ISuperblock *sb) {
        if (_mounted == sb) {
            _mounted = nullptr;
        }
        void_return();
    }

    Result<DevFSSuperblock *> DevFSDriver::mounted_superblock() const {
        if (_mounted == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return _mounted;
    }
}  // namespace devfs
