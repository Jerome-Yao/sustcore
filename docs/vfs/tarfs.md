# TarFS

本文总结 `kernel/vfs/tarfs.h` 与 `kernel/vfs/tarfs.cpp` 的实现。TarFS 是当前 initrd 使用的只读 tar 文件系统，它把 tar archive 中的 header 块映射成 VFS inode。

## 总体模型

TarFS 基于 ustar 格式:

- 固定块大小为 `512` 字节。
- 每个文件或目录由一个 tar header 块描述。
- 普通文件的数据紧跟在 header 后。
- 下一个 header 的位置由文件大小向上取整到 512 字节块后得到。

当前 TarFS 不构建独立 dentry 树，而是直接扫描 tar 数据并按 header 偏移定位文件。

## 关键结构

### `TarBlock`

`TarBlock` 是一个 512 字节 union:

- `header`: ustar header 视图。
- `raw`: 原始字节视图。

它提供:

- `is_header()`: 检查 magic 是否为 `"ustar"`。
- `is_empty()`: 检查整个块是否全为 0。
- `calc_checksum()`: 按 tar 规则计算 checksum。

### `parse_octal`

tar header 中的大小、checksum 等字段使用 ASCII 八进制表示。`parse_octal()` 用模板适配不同长度字段，遇到 `'\0'` 或非八进制字符时停止解析。

## inode 编号

TarFS 使用 header 块偏移派生 inode id:

```cpp
inode_t inode_id = offset / BLOCK_SIZE;
```

对应的反向转换是:

```cpp
size_t offset = inode_id * BLOCK_SIZE;
```

这样每个 tar header 的 inode id 都稳定且唯一。根目录当前固定为 inode `0`，也就是 archive 起始 header。

## 文件对象

### `TarFile`

`TarFile` 继承 `IFile`。

构造时会:

1. 保存所属 `TarSuperblock`。
2. 保存 header 指针。
3. 将文件数据起点设为 `header + 512`。
4. 从 header 的 size 字段解析文件长度。
5. 计算文件数据结束地址。

接口行为:

- `read(offset, buf, len)`: 只读文件数据范围内可用部分，返回实际读取字节数。
- `write(...)`: 返回 `ErrCode::NOT_SUPPORTED`。
- `size()`: 返回文件长度。
- `sync()`: 返回 `ErrCode::NOT_SUPPORTED`。

`read()` 会拒绝负 offset，以及超出文件末尾的 offset。

### `TarDirectory`

`TarDirectory` 继承 `IDirectory`。

`lookup(name)` 的行为是:

1. 空名称直接返回当前目录 inode id。
2. 用当前目录 header 的 `name` 与输入 `name` 拼接，得到目标路径。
3. 规范化目标路径。
4. 从当前目录之后的 tar header 开始线性扫描。
5. 对每个 header 的 `name` 规范化后与目标路径比较。
6. 命中则返回该 header 偏移对应的 inode id。
7. 扫描失败返回 `ErrCode::ENTRY_NOT_FOUND`。

`sync()` 返回 `ErrCode::NOT_SUPPORTED`。

## Superblock

`TarSuperblock` 继承 `ISuperblock`，持有:

- `data_`: tar archive 数据起点。
- `size_`: archive 总大小。
- `fs_`: 所属 `TarFSDriver`。
- `device_`: 底层块设备。
- `sb_id_`: superblock id。

### `get_inode(inode_id)`

获取 inode 的流程是:

1. 将 inode id 转成 tar 数据偏移。
2. 检查偏移不越界。
3. 检查该位置确实是 tar header。
4. 根据 `typeflag == '5'` 判断目录。
5. 目录创建 `TarDirectory`，否则创建 `TarFile`。
6. 返回 `util::owner<IINode *>`。

`TarSuperblock::sync()` 返回 `ErrCode::NOT_SUPPORTED`，因为当前 TarFS 是只读文件系统。

### 数据所有权

TarFS 挂载 RamDisk 时直接引用 `RamDiskDevice::base()`，不复制数据。此时 superblock 析构不会释放 archive 数据。

挂载非 RamDisk 设备时，驱动会分配一段内存，把整个设备读入其中。此时 `TarSuperblock` 析构会释放该缓冲区。

## 驱动

### `TarFSDriver::probe`

`probe(device, options)` 会:

1. 读取块大小和块数量。
2. 分配 `block_sz * block_cnt` 字节缓冲区。
3. 从设备读入全部块。
4. 调用 `is_valid()` 校验 tar 数据。
5. 释放临时缓冲区。

如果读块数量不等于请求块数量，返回 `ErrCode::IO_ERROR`。校验失败返回 `ErrCode::INVALID_PARAM`。

### `TarFSDriver::mount`

`mount(device, options)` 会:

1. 读取设备总大小。
2. 若设备是 `RamDiskDevice`，直接使用其底层内存。
3. 否则分配内存并读入整个设备。
4. 创建 `TarSuperblock`。
5. 返回 superblock owner。

当前 `options` 未使用。

### `is_valid`

`is_valid(size, data)` 检查:

- archive 大小必须是 512 字节整数倍。
- 每个非空 header 的 stored checksum 必须等于计算结果。
- 空块允许存在。

验证过程会按文件大小跳到下一个 header。

## 内存池

`TarFile` 和 `TarDirectory` 使用 KOP 专用对象池:

- `kop::TarFile`
- `kop::TarDirectory`

`tarfs::init_kop()` 负责 placement new 初始化这两个对象池。`operator new/delete` 会转发到对应 KOP。

## 当前限制

TarFS 当前限制如下:

- 只读，不支持写入、同步、创建或删除。
- 目录 lookup 是线性扫描。
- 没有完整 dentry 树或目录项列表接口。
- 根目录固定为 inode `0`，要求 archive 起始块是根目录 header。
- 挂载时会整体读取非 RamDisk 设备，未使用按需块缓存。
- `unmount()` 当前为空操作，主要清理由 superblock 析构完成。
