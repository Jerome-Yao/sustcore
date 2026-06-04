# VFile Object

本文总结 `kernel/object/vfile.*` 与 `kernel/vfs/vfs.*` 的关系。`VFile` capability 是当前文件访问能力的封装层，所有文件 I/O 都经由 VFS 转发。

## 对象来源

`VFileObject` 操作的底层 payload 定义在 [kernel/vfs/vfs.h](/home/flysong/Sustcore/kernel/vfs/vfs.h):

```cpp
class VFile : public cap::_PayloadHelper<PayloadType::VFILE>
```

因此:

- `VFile` 既是 VFS 内部文件句柄, 也是 capability payload
- `VFileObject` 只是它的权限封装与能力接口

## `VFile` payload

`VFile` 内部持有:

- `owner<VINode *> _vind`
- `util::Path _mount_path`
- `VFS *_vfs`

其中:

- `_vind` 表示该打开文件对应的 vnode
- `_mount_path` 用于在销毁时回到 VFS 维护活跃打开计数
- `_vfs` 指向创建该文件对象的 VFS 实例

### 销毁语义

`VFile::destruct()` 会:

1. 通知 VFS 递减对应挂载点的活跃文件计数
2. 直接 `delete this`

析构函数会释放其持有的 `VINode`。

因此当前 `VFile` 的生命周期完全由 capability payload 引用计数驱动, 不再依赖额外的缓存整理步骤。

## `VFileObject`

`VFileObject` 继承自 `CapObj<::VFile>`。

它不再额外缓存 `VINode *`, 只保留从 capability 解析出的 `_obj` 指针, 并在每次操作时将该 `VFile` 交给 VFS。

### 提供的接口

- `read(offset, buf, len)`
- `write(offset, buf, len)`
- `size()`
- `sync()`
- `read_exact(offset, buf, len)`

## 权限模型

定义在 `perm::vfile`:

- `READ`
- `WRITE`
- `EXEC`

当前代码里实际用到的是:

- `READ`
- `WRITE`

`EXEC` 目前尚未在 `VFileObject` 方法中直接使用。

## `read()`

要求 `READ` 权限。

成功后调用:

```cpp
VFS::inst().read(*_obj, offset, buf, len)
```

## `write()`

要求 `WRITE` 权限。

成功后调用:

```cpp
VFS::inst().write(*_obj, offset, buf, len)
```

## `size()`

要求 `READ` 权限。

成功后调用:

```cpp
VFS::inst().size(*_obj)
```

这里把“查询文件大小”视为读权限的一部分。

## `sync()`

要求 `WRITE` 权限。

成功后调用:

```cpp
VFS::inst().sync(*_obj)
```

说明同步刷新被视为一种修改型操作, 而不是纯查询操作。

## `read_exact()`

`read_exact()` 是一个便捷包装:

1. 调用 `read()`
2. 若失败则传播错误
3. 若返回长度不等于请求长度, 则返回 `IO_ERROR`

因此它适合用于需要完整读取固定长度头部或结构体的路径。

## 与 VFS 的关系

`VFileObject` 自身不保存文件偏移, 也不保存打开模式状态。它只是:

- 通过 capability 获取 `VFile` payload
- 把请求转发给 VFS 全局单例
- 在转发前做 capability 权限检查

VFS 才是真正执行文件 I/O 的统一边界。对象层不直接操作 `VINode` 或 `IFile`。

## 生命周期关系

链路大致是:

1. VFS 打开路径并构造 `VFile`
2. `VFile` 独占持有对应 `VINode`
3. capability 引用 `VFile`
4. `VFileObject` 只是 capability 的栈上包装
5. 当最后一个 `VFILE` capability 被移除时, payload 进入 `destruct()`
6. VFS 活跃计数减少, `VINode` 随 `VFile` 析构释放

因此文件对象的最终生命周期受 capability 持有数量控制。

## 当前设计特点

VFile capability 的特点是:

- **文件 I/O 统一经由 VFS**
- **对象层极薄, 只做权限检查与转发**
- **`VINode` 只保留在 VFS 内部与 payload 私有状态中**
- **适合和 capability 传递结合, 作为进程间可转交文件句柄**

它本质上是“VFS 文件句柄的 capability 封装”。
