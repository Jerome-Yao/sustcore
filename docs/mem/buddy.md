# Buddy Page Allocator

本文说明 buddy 页框分配器。相关实现位于 `kernel/mem/buddy.*`，统一入口由 `GFP` 提供。

## 角色

Buddy 分配器负责管理空闲物理页框。它只关心“哪些物理页空闲、如何分裂和合并”，不负责用户映射、对象构造或 COW 引用计数。

在 buddy + slub 模型中:

```text
GFP
  -> BuddyAllocator
      -> physical pages
```

`GFP` 在 buddy 之上叠加物理页引用计数，用于 Memory payload 和 COW。

## 数据结构

每个空闲块开头被 placement-new 成 `FreeBlock`:

```cpp
struct FreeBlock {
    util::ListHead<FreeBlock> list_head;
};
```

空闲链表:

```cpp
static BlockList free_area[MAX_BUDDY_ORDER + 1];
```

其中 `BlockList` 是按地址排序的 `OrderedIntrusiveList`。每个 order 表示大小为:

```text
2^order pages
```

当前最大 order:

```cpp
MAX_BUDDY_ORDER = 15
```

即最大块大小为 `2^15` 页。

## 初始化

`pre_init()`:

- 初始化所有 order 的空闲链表。
- 遍历 `MemRegion[]`。
- 对每个 FREE 区域执行页对齐。
- 调用 `add_memory_range<PRE_INIT>()` 加入 buddy。

`post_init()`:

- 把链表节点和哨兵指针从 PA 访问语义迁移到 KPA 访问语义。
- 打印迁移后的空闲布局。

## 添加内存范围

`add_memory_range(paddr, pages)` 会把任意页数的连续范围拆成尽可能大的对齐 buddy 块:

1. 从当前地址开始。
2. 找到不超过剩余页数且满足对齐的最大 order。
3. 调用 `put_page_in_order()` 放入对应链表。
4. 前进到下一段，直到处理完。

这让非 2 的幂长度的内存区间也能自然进入 buddy。

## 分配流程

`get_free_page(frame_count)`:

1. 检查 `frame_count != 0`。
2. 检查请求不超过最大 order。
3. 通过 `pages2order(frame_count)` 向上取到最小可容纳 order。
4. `fetch_frame_order(order)` 从当前或更高 order 找空闲块。
5. 如果拿到更大块，则不断二分，把右半边放回 buddy。
6. 如果分配块大于请求页数，把多余页通过 `add_memory_range()` 归还。

`get_free_pages_in_order(order)` 直接按 order 分配。

## 释放流程

`put_page(paddr, frame_count)`:

- 空地址或 0 页数直接返回。
- 检查页对齐。
- 通过 `add_memory_range()` 按对齐块释放。

`put_page_in_order(paddr, order)`:

1. 在释放块起始地址 placement-new `FreeBlock`。
2. 插入对应 order 的有序链表。
3. 判断 buddy 位于左侧还是右侧。
4. 如果相邻 buddy 块也空闲且地址连续，移除两块并合并成更高 order。
5. 重复合并直到无法合并或达到最大 order。

## 与 GFP 的关系

`GFP::get_free_page()` 调用底层 `RawGFPImpl::get_free_page()` 后，会把每个返回页的引用计数设为 1。

`GFP::put_page()` 会先减少引用计数，只把引用计数降到 0 的连续页段交还 buddy。

`GFP::keep_page()` 增加引用计数，主要用于 COW 共享页。

## 测试覆盖点

`kernel/test/buddy.cpp` 覆盖:

- 混合大小分配与碎片重用。
- order 0 耗尽后反向释放合并。
- 大块分割与再次合并。
- 参数合法性。
- 不同 order 的地址对齐。
- 小规模压力分配和释放。

## 注意事项

- buddy 元数据直接存放在空闲物理块中，分配出去后该区域不再是 `FreeBlock`。
- 释放的地址必须页对齐，且页数必须与调用方实际持有的范围一致。
- `put_page()` 本身不检查双重释放；调用方需要保证生命周期正确。
- 若 `RawGFPImpl` 未切换到 `BuddyAllocator`，这些释放和合并语义不会成为 GFP 的实际行为。
