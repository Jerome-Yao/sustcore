# Memory And Capability Interaction

本文说明内存子系统与能力系统的交互。相关实现位于 `kernel/object/memory.*`、`kernel/mem/vma.*` 和 `kernel/syscall/memory.cpp`。

## Memory capability 模型

`cap::MemoryPayload` 表示一段承诺大小的内存对象:

- `memsz`: 承诺大小。
- `shared`: clone 时是否共享同一 payload 和物理页。
- `continuity`: 是否要求物理页连续。
- `growth`: 允许的增长/收缩方式。
- `phy_pages`: 已实际分配物理页表，key 为 Memory 内页号。

它不是直接的虚拟映射。虚拟映射由 `MemoryObject::map_into()` 在目标 `TaskMemoryManager` 中创建 VMA。

## 物理页记录

`PhyPage` 保存:

- `addr`: 物理页地址。
- `refcount`: 当前 payload 视角下的 COW 共享计数。

注意这不同于 `GFP` 的全局物理页引用计数。payload 内 refcount 用于判断当前 Memory 是否需要 fork；GFP refcount 用于决定物理页何时真正归还底层页框分配器。

## 懒分配

`MemoryPayload::ensure_page(offset)`:

1. 先调用 `lookup_page(offset)`。
2. 如果页已存在，直接返回物理地址。
3. 如果是 `PAGE_NOT_PRESENT`，调用 `GFP::get_free_page(1)`。
4. 把新页清零。
5. 在 `phy_pages` 中记录 `{paddr, 1}`。

这使 Memory capability 可以表示大范围承诺内存，而只为实际访问的页面分配物理页。

## 读写 payload

`read(offset, data, len)`:

- 检查边界。
- 分页处理跨页访问。
- 未分配页会先 `ensure_page()`，因此读未写过的页会得到零页内容。

`write(offset, data, len)`:

- 检查边界。
- 分页处理跨页访问。
- 未分配页会先分配。
- 如果目标页 refcount 大于 1，先 `fork()` 拆分，再写入。

## Clone 与 COW

`clone_payload()`:

- shared Memory: 返回自身。
- 非 shared Memory: 创建新 `MemoryPayload`，复制 `phy_pages` 记录，对每个已有页调用 `GFP::keep_page()`，并增加 payload 内页 refcount。

fork 任务时，`TaskMemoryManager::clone_to_cow()` 会把页表中的可写页降为只读并标记 COW。之后任一进程写入时触发 `on_wp()`，最终调用 `MemoryPayload::fork()`。

`fork(offset)`:

- refcount <= 1 时只把 refcount 归一。
- refcount > 1 时分配新页，复制旧页内容，替换当前 payload 的 `PhyPage.addr`，并对旧页调用 `GFP::put_page()`。

## resize

`MemoryPayload::resize(newsz)`:

- shared Memory 暂不支持 resize。
- 根据 `growth` 判断是否允许增长或收缩。
- 非连续 Memory 收缩时释放超出范围的已分配页。
- 连续 Memory 会重新分配连续物理区，复制保留页内容，再释放旧页。

`MemoryObject::resize_in(tmm, newsz)` 会在调整 payload 后同步相关 VMA。

## `MemoryObject`

`MemoryObject` 是 `CapObj<MemoryPayload>` 封装，负责权限检查。

核心操作:

- `map_into(tmm, vaddr, rwx, growth)`
- `unmap_from(tmm, vaddr)`
- `resize_in(tmm, newsz)`
- `query()`

权限检查集中在对象方法内，syscall 层主要负责 lookup capability 和搬运用户参数。

## 映射权限

`map_into()` 会检查:

- 是否有 `perm::memory::MAP`。
- 请求页权限是否至少可读。
- 是否有 `READ` 权限。
- 请求写映射时是否有 `WRITE` 权限。
- 请求执行映射时是否有 `EXEC` 权限。
- 请求弹性增长/收缩时是否有对应 capability 权限。

通过后，它会在目标 `TaskMemoryManager` 中创建 VMA。物理页仍按缺页懒分配。

## 与 VMA 的关系

一个 `MemoryPayload` 可以被一个或多个 VMA 引用。VMA 保存:

- 映射虚拟区间。
- 映射权限。
- Memory 内偏移。
- 增长方式。

当 Memory 收缩或 resize 时，`TaskMemoryManager` 会解除尾部超出范围的页表映射，并同步 VMA 区间。

## 销毁语义

`MemoryPayload::destruct()` 会遍历 `phy_pages`，对每个物理页调用 `GFP::put_page()`，最后 `delete this`。

VMA 析构只释放对 payload 的引用，不直接释放物理页。物理页生命周期由 Memory payload 的引用计数和 capability 引用共同决定。
