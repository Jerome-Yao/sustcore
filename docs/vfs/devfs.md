# DevFS

本文总结 `kernel/vfs/device.h` 中的设备文件系统实现。DevFS 是当前 VFS 下的伪文件系统，用于把内核设备暴露为文件节点。

## 总体模型

DevFS 不依赖块设备，继承 `IPesudoFsDriver`。当前启动过程中它被注册为 `"devfs"`，并挂载到 `/sys/`。

DevFS 的对象层次是:

1. `DevFSDriver`: 文件系统驱动和单挂载状态。
2. `DevFSSuperblock`: 单次挂载实例，维护目录和字符设备工厂。
3. `DevFSDirectory`: 目录 inode。
4. `CharDevFile`: 字符设备文件基类。

## 字符设备工厂

字符设备通过 `CharFactory` 延迟创建:

```cpp
struct CharFactory {
    void *ctx;
    Result<util::owner<IINode *>> (*create)(void *ctx, inode_t inode_id);
};
```

DevFS superblock 只保存 inode id 到 factory 的映射。VFS 缓存未命中并调用 `get_inode()` 时，DevFS 会通过 factory 创建一个新的 `CharDevFile` 对象；若该 inode 已被 VFS 缓存命中，则不会再次触发 factory。

## `CharDevFile`

`CharDevFile` 继承 `IFile`，用于实现设备文件。

它提供两个偏移无关的设备接口:

- `read(void *buf, size_t len)`
- `write(const void *buf, size_t len)`

并把 VFS 的 offset 形式适配过去:

- `read(off_t offset, ...)` 要求 `offset == 0`，否则返回 `ErrCode::INVALID_PARAM`。
- `write(off_t offset, ...)` 要求 `offset == 0`，否则返回 `ErrCode::INVALID_PARAM`。

默认行为:

- `read(void *, size_t)` 返回 `ErrCode::NOT_SUPPORTED`。
- `write(const void *, size_t)` 是纯虚函数，具体设备必须实现。
- `size()` 返回 `0`。
- `sync()` 成功返回。
- `inode_cache()` 返回 `INodeCachePolicy::PERMANENT`。
- `file_cache()` 返回 `FileCachePolicy::NONE`。

`FileCachePolicy::NONE` 会让 VFS 在读写后调用 `sync()`。对于大多数字符设备，当前 `sync()` 是空操作。

## `DevFSDirectory`

`DevFSDirectory` 继承 `IDirectory`，持有:

- `_inode_id`
- `_name`
- `_entries`: `name -> inode_t`

主要接口:

- `lookup(name)`: 查找目录项，不存在返回 `ErrCode::ENTRY_NOT_FOUND`。
- `add_entry(name, inode_id)`: 添加目录项，重复名称返回 `ErrCode::KEY_DUPLICATED`。
- `sync()`: 成功返回。
- `inode_cache()`: 返回 `INodeCachePolicy::PERMANENT`。

当前 DevFS 目录只支持查找和添加，不提供删除或遍历接口。

## `DevFSSuperblock`

`DevFSSuperblock` 继承 `ISuperblock`，维护 DevFS 的所有运行时节点。

内部状态:

- `_fs`: 所属 `DevFSDriver`。
- `_sb_id`: superblock id。
- `_next_inode`: 下一个可分配 inode id，初始为 `1`。
- `_root`: 根目录，inode id 为 `0`。
- `_directories`: `inode_t -> owner<DevFSDirectory *>`。
- `_char_factories`: `inode_t -> CharFactory`。

### 根目录

构造函数会创建根目录:

```cpp
_root = new DevFSDirectory(0, "/");
```

并将它插入 `_directories`。

### inode 获取

`get_inode(inode_id)` 的规则:

1. 如果 inode id 属于 `_directories`，返回对应目录对象。
2. 如果 inode id 属于 `_char_factories`，调用 factory 创建字符设备文件。
3. 都不命中则返回 `ErrCode::ENTRY_NOT_FOUND`。

目录对象由 superblock 持有。字符设备文件由 factory 创建并交给 VFS 层接管。

### 目录与设备注册

`mkdir(parent_inode, name)`:

- 要求父 inode 是已存在目录。
- 分配新 inode id。
- 创建 `DevFSDirectory`。
- 向父目录添加目录项。
- 把目录插入 `_directories`。

`register_char(parent_inode, name, factory)`:

- 要求父 inode 是已存在目录。
- 分配新 inode id。
- 向父目录添加目录项。
- 把 factory 插入 `_char_factories`。

`ensure_dir(parent_inode, name)`:

- 若父目录下已有同名项，直接返回其 inode id。
- 若不存在，则调用 `mkdir()` 创建目录。
- 其它 lookup 错误会继续传播。

## `DevFSDriver`

`DevFSDriver` 继承 `IPesudoFsDriver`。

它固定文件系统名称:

```cpp
const char *name() const final {
    return "devfs";
}
```

### probe

`probe(name, options)` 只接受 `"devfs"`，否则返回 `ErrCode::NOT_SUPPORTED`。当前 `options` 未使用。

### mount

`mount(name, options)` 的行为:

- 只接受 `"devfs"`。
- 如果已经挂载，返回 `ErrCode::BUSY`。
- 创建新的 `DevFSSuperblock`。
- 记录 `_mounted`。
- 返回 superblock owner。

当前 DevFS 是单实例挂载模型，同一 `DevFSDriver` 不能同时挂载多个 superblock。

### unmount

`unmount(sb)` 会在参数等于 `_mounted` 时清空 `_mounted`，然后成功返回。

### mounted_superblock

`mounted_superblock()` 用于其它内核组件取得当前 DevFS superblock。如果尚未挂载，返回 `ErrCode::ENTRY_NOT_FOUND`。

## 设备接入方式

设备驱动想暴露字符设备时，一般流程是:

1. 取得 `DevFSDriver::mounted_superblock()`。
2. 用 `ensure_dir()` 创建或查找设备目录。
3. 构造 `CharFactory`，保存设备上下文和创建函数。
4. 调用 `register_char()` 在目录下注册字符设备文件。

当前 `kernel/driver/serial.*` 中已有串口设备挂载到 DevFS 的接口雏形，但主初始化路径里相关调用仍处于注释状态。

## 当前限制

DevFS 当前限制如下:

- 只支持单次挂载。
- 没有目录遍历接口。
- 没有删除目录或注销字符设备接口。
- `CharDevFile` 只接受 offset 为 0 的读写。
- 字符设备节点通过 factory 动态创建，具体设备需要自行保证上下文生命周期有效。
