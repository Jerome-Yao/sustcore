# Memory Subsystem

本文档汇总 Sustcore 内存子系统，覆盖物理内存探测、地址类型、页表、内核地址空间、页框分配、对象分配、用户地址空间、缺页处理、COW 以及 Memory capability 交互。

本文按当前设计假设说明内存分配组合:

- 页框分配层: `GFP -> BuddyAllocator`
- 小对象/内核堆层: `Allocator/KOP -> slub::MixedSizeAllocator`

源码中的实际组合由别名控制:

- `kernel/mem/gfp.h` 中的 `RawGFPImpl`
- `kernel/mem/alloc.h` 中的 `Allocator`

如果别名仍指向 `LinearGrowGFP` 或 `LinearGrowAllocator`，说明当前构建仍处在线性分配器路径；本文的 buddy/slub 章节描述的是切换到 buddy + slub 后的预期主路径。

## 文档索引

- [总览](overview.md)
- [初始化流程](initialization.md)
- [地址模型](addressing.md)
- [SV39 页表管理](paging.md)
- [内核地址空间](kernel_space.md)
- [Buddy 页框分配器](buddy.md)
- [SLUB 对象分配器](slub.md)
- [任务地址空间与 VMA](vma.md)
- [用户态内存访问](userspace.md)
- [与能力系统的交互](capability_interaction.md)

## 主要源码位置

- `include/sustcore/addr.h`: 地址类型、地址范围、地址转换和页对齐。
- `kernel/arch/riscv64/device/memory.cpp`: RISC-V 内存布局探测。
- `kernel/arch/riscv64/mem/sv39.h`: SV39 页表管理器。
- `kernel/mem/kaddr.*`: 内核物理段与内核虚拟映射。
- `kernel/mem/gfp.*`: GFP 统一页框分配接口。
- `kernel/mem/buddy.*`: Buddy 页框分配器。
- `kernel/mem/slub.*`: SLUB slab/object 分配器。
- `kernel/mem/alloc.*`: 全局对象分配器与 KOP 绑定。
- `kernel/mem/vma.*`: 任务地址空间、VMA、缺页与 COW。
- `kernel/mem/userspace.*`: 内核访问用户地址空间的辅助接口。
- `kernel/object/memory.*`: Memory capability payload 与对象操作。
