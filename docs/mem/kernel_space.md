# Kernel Address Space

本文说明内核地址空间布局和映射方式。相关实现位于 `kernel/mem/kaddr.*` 和 `kernel/main.cpp`。

## 内核段

`ker_paddr::Segment` 描述一段物理地址和虚拟地址的对应关系:

```cpp
struct Segment {
    PhyAddr pstart, pend;
    VirAddr vstart, vend;
};
```

当前维护的段:

- `kernel`
- `text`
- `rodata`
- `data`
- `bss`
- `misc`
- `kphy_space`

这些段由 linker symbols 构造:

- `skernel` / `ekernel`
- `s_text` / `e_text`
- `s_rodata` / `e_rodata`
- `s_data` / `e_data`
- `s_bss` / `e_bss`
- `s_misc`

## KVA 段

`make_kva_seg()` 把内核镜像物理区间映射到 `KVA_OFFSET` 高半区:

```cpp
VirAddr vs = ps + KVA_OFFSET;
VirAddr ve = pe + KVA_OFFSET;
```

内核代码、只读数据、数据段、BSS 和 misc 区域都通过这种方式构造。

## KPA 线性物理映射

`kphy_space` 使用 `make_kpa_seg()` 构造:

```cpp
VirAddr vs = ps + KPA_OFFSET;
VirAddr ve = pe + KPA_OFFSET;
```

它覆盖 `env::meminfo().lowpm` 到 `env::meminfo().uppm` 的物理内存范围，使内核可以通过 KPA 访问物理页内容。

## 映射权限

`ker_paddr::mapping_kernel_areas(man)` 映射内核段:

- `text`: `R-X`
- `rodata`: `R--`
- `data`: `RW-`
- `bss`: `RW-`
- `misc`: `R--`
- `kphy_space`: `RW-`

这些映射都设置:

- `u = false`
- `g = true`

`g = true` 表示全局映射，适合所有地址空间共享的内核区域。

## 用户态可访问低端内存

`post_init()` 中会对低端内存映射设置 `U` 位:

```cpp
kernelman.modify_range_flags<PageMan::ModifyMask::U>(
    meminfo.lowvm,
    meminfo.uppm - meminfo.lowpm,
    PageMan::RWX::NONE,
    true,
    false);
```

这一步使低端映射可被用户态访问。由于它操作的是内核主页表，必须谨慎看待安全边界；后续用户任务仍通过各自 `TaskMemoryManager` 管理 VMA 和页表。

## 每个任务页表中的内核映射

`TaskMemoryManager` 构造新页表时会:

```cpp
PageMan::make_root(_pgd);
ker_paddr::mapping_kernel_areas(_pman);
```

因此每个任务地址空间都带有内核高半区和 KPA 映射，用户区 VMA 映射则按需建立。

## 注意事项

- 内核段映射权限应尽量保持最小权限，尤其是 `text` 不应可写。
- `kphy_space` 是调试和页表/物理页访问的基础，不代表用户可任意访问物理内存。
- 新增内核段或修改 linker symbols 后，应同步检查 `ker_paddr::init()` 与映射权限。
- `mapping_kernel_areas()` 当前每个页表都会重新构造内核映射，源码中已有 TODO 提示未来可复用专门的内核页表。
