# Userspace Memory Access

本文说明内核访问用户地址空间的辅助接口。相关实现位于 `kernel/mem/userspace.*`。

## 背景

RISC-V S-mode 默认不能直接访问 U-mode 页面。内核在复制 syscall 参数、读写用户缓冲区或访问非当前任务地址空间时，需要显式处理:

- 当前页表是否就是目标用户地址空间。
- 是否需要打开 `sstatus.SUM`。
- 是否需要临时切换页表根。

## SUM 控制

`uspace::sum_open()`:

- 读取 `sstatus`。
- 设置 `sum = 1`。
- 写回 `sstatus`。

`uspace::sum_close()` 把 `sum` 清 0。

`SumGuard` 是作用域守卫:

- `open()` 打开 SUM。
- 析构时自动 `close()`。
- 禁止复制、移动、new/delete。

推荐用法:

```cpp
uspace::SumGuard guard;
guard.open();
memcpy(kbuf, uaddr.addr(), len);
```

## 当前地址空间判断

`is_current_uspace(utmm)` 返回 true 的条件:

- `utmm == env::inst().tmm()`，或
- `utmm->pgd()` 与 `env::inst().pgd()` 相同。

当前地址空间快路径可以直接打开 SUM 后访问用户虚拟地址。

## 跨地址空间访问

非当前地址空间使用 `UspaceSwitchGuard`:

1. 保存当前 `env::tmm` 和当前页表根。
2. 更新 `env::tmm` 为目标地址空间。
3. 切换到目标 `TaskMemoryManager` 的页表根。
4. 刷新 TLB。
5. 析构时恢复原地址空间和页表根，并再次刷新 TLB。

随后再配合 `SumGuard` 访问目标用户地址。

## 复制接口

复制方向:

```cpp
enum class CpyDir {
    U2K,
    K2U,
};
```

模板接口:

```cpp
template <CpyDir Dir>
Result<void> umemcpy(TaskMemoryManager *utmm, void *kbuf,
                     VirAddr uaddr, size_t len);
```

行为:

- 检查 `utmm` 非空。
- `len != 0` 时检查 `kbuf` 非空。
- 如果是当前地址空间，打开 SUM 后直接 `memcpy`。
- 否则调用 `umemcpy_other()` 临时切换页表后复制。

## 填充接口

`umemset(utmm, uaddr, value, len)`:

- 当前地址空间: 打开 SUM 后直接 `memset`。
- 非当前地址空间: 调用 `umemset_other()` 临时切换页表。

## 错误处理

常见错误:

- `NULLPTR`: `utmm == nullptr` 或必要缓冲区为空。
- `INVALID_PARAM`: 目标地址空间没有有效 `pgd()`。

这些接口本身不会完整验证用户地址范围是否由 VMA 覆盖。实际访问时若目标页不存在，硬件异常会进入缺页处理路径。

## 注意事项

- `SumGuard` 的作用域必须尽量小，只包裹实际用户内存访问。
- 临时切换页表期间必须保证不会破坏当前 hart 的调度/中断不变量。
- 这些接口只处理内核与用户地址间的数据搬运，不表达用户缓冲区权限；权限和映射由 VMA、页表和 syscall 层共同保证。
- 不要在打开 SUM 后执行复杂逻辑或调用可能阻塞的流程。
