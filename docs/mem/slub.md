# SLUB Object Allocator

本文说明 SLUB 对象分配器。相关实现位于 `kernel/mem/slub.*`，全局分配器绑定位于 `kernel/mem/alloc.h`。

## 角色

SLUB 用于在页框分配器之上分配内核对象:

```text
GFP -> BuddyAllocator -> pages
SLUB -> object caches -> kernel objects
```

它避免每个小对象都单独占用页框，并通过 freelist 快速复用对象槽。

## Slab 布局

当前常量:

```cpp
PAGES_PER_SLAB = 1
SLAB_BYTES = PAGESIZE
ALIGN = 16
SLAB_KMAX = 2048
```

每个 slab 页开头放置 `SlabHeader`:

```cpp
struct SlabHeader {
    ListHead<SlabHeader> list_head;
    void *freelist;
    size_t inuse;
    size_t total;
    SlabState state;
};
```

对象槽从 `SlabHeader` 后按对象对齐开始排列。空闲对象的前几个字节存放 next 指针，组成单向 freelist。

## Slab 状态

每个 `SlubAllocator<T>` 维护三条侵入式链表:

- `empty`: 没有对象被使用。
- `partial`: 部分对象被使用。
- `full`: 所有对象被使用。

分配时优先从 `partial` 取 slab，其次 `empty`，最后新建 slab。

释放时根据 `inuse` 变化在三条链表之间迁移:

- `inuse == 0`: 转入 `empty`。
- 从 full 释放一个对象: 转入 `partial`。
- 分配后 `inuse == total`: 转入 `full`。

## 小对象分配

`SlubAllocator<T>::alloc()`:

1. 选择 partial/empty/new slab。
2. 从 slab 的 freelist 弹出一个对象槽。
3. 更新 `inuse` 与全局对象计数。
4. 必要时把 slab 转入 full。
5. 返回对象指针。

`free(ptr)`:

1. 通过 `slab_of(ptr)` 按 slab 大小向下对齐找到 `SlabHeader`。
2. 把对象重新压入 freelist。
3. 更新 `inuse` 和状态链表。

## 大对象路径

当对象大小 `sizeof(T) >= SLAB_KMAX` 时，`SlubAllocator<T>` 使用特化版本:

- 按 `page_align_up(sizeof(T)) / PAGESIZE` 计算页数。
- 直接通过 `GFP::get_free_page(pages)` 分配。
- 释放时通过 `GFP::put_page(paddr, pages)` 归还。

这避免大对象占用普通 slab 并造成严重内部碎片。

## `FixedSizeAllocator`

`FixedSizeAllocator<sz>` 为固定大小对象提供一个静态 `SlubAllocator<Object>`。

支持:

- `malloc()`
- `free(ptr)`
- `init()`

`sz` 必须是 2 的幂。

## `MixedSizeAllocator`

`MixedSizeAllocator` 是面向通用 `Allocator::malloc(size)` 的混合大小分配器。

当前支持大小级别:

```text
8, 16, 32, 64, 128, 256, 512, 1024, 2048
```

分配流程:

1. 把请求大小向上取到 2 的幂，最小为 8。
2. 小于 `KMAX` 时进入对应 `FixedSizeAllocator`。
3. 大于等于 `KMAX` 时走 `large_malloc()`，直接向 GFP 请求页。
4. 使用 `AllocRecord` 记录原指针和实际分配大小。

释放流程:

1. 通过 `alloc_records` 查找 `AllocRecord`。
2. 根据记录的实际大小选择小对象 cache 或大对象 GFP 释放。
3. 移除并释放 `AllocRecord`。

`AllocRecord` 自身也由一个 `SlubAllocator<AllocRecord>` 管理。

## 与 `Allocator` / `KOP` 的关系

`kernel/mem/alloc.h` 定义全局对象分配器:

```cpp
using Allocator = ...;

template <typename ObjType>
using KOP = SimpleKOP<ObjType, Allocator>;
```

按 buddy + slub 模型，`Allocator` 应指向 `slub::MixedSizeAllocator`。这样 `KOP<T>` 会通过 SLUB 分配对象。

如果 `Allocator` 指向 `LinearGrowAllocator`，则 `free()` 不会真正释放小对象，SLUB 测试中的全局分配记录也不代表真实主路径。

## 统计信息

`SlubStats` 包含:

- `total_slabs`
- `objects_inuse`
- `objects_total`
- `memory_usage_bytes`

可用于测试和调试对象缓存状态。

## 测试覆盖点

`kernel/test/slub.cpp` 覆盖:

- 小对象分配与释放。
- 释放后对象槽复用。
- 大对象路径。
- 多 slab 扩张。
- freelist 一致性压力测试。
- `Allocator` 混合大小分配记录回归。

## 注意事项

- SLUB 不调用对象构造/析构；它只提供原始对象槽。调用方需要自行 placement-new 或保证类型可直接使用。
- 当前 mixed allocator 的分配记录用侵入式链表线性查找，规模很大时性能有限。
- `SlubAllocator<T>::free(nullptr)` 会记录 warning 并返回。
- slab 页来自 GFP，因此底层页框分配器必须已经初始化。
