# Map, Queue And Std-Compatible Containers

本文说明 `include/sus/map.h`、`include/sus/queue.h` 以及 `include/std/c++/` 下几个常用标准库兼容容器。

## `util::LinkedMap`

`LinkedMap<K, V, Equality>` 是基于侵入式链表 entry 的线性映射。每个 entry 内部保存:

```cpp
util::Pair<K, V> pair;
util::ListHead<Entry> list_head;
```

核心接口:

- `get(key)` 返回 `std::expected<std::reference_wrapper<V>, bool>`
- `get_entry(key)` 返回键值对引用
- `put(key, value)` 插入或覆盖
- `remove(key)`
- `contains(key)`
- `size()`、`empty()`
- 双向迭代器

`LinkedMap` 适合元素数量较小、插入删除频率不高且希望保持简单实现的场景。查找是线性的，不适合大量键或热点查询。

## `util::StaticArrayQueue`

`StaticArrayQueue<T, Capacity>` 是固定容量环形队列。实现使用 `head/tail` 和静态数组，保留一个空槽用于区分满和空。

核心接口:

- `size()`、`empty()`、`full()`、`capacity()`
- `push(value)`、`emplace(args...)`
- `pop()`
- `front()`
- `clear()`
- `contains(value)`

错误处理:

- 满队列 `push()` 或 `emplace()` 返回 `std::error_type::OVERFLOW_ERROR`
- 空队列 `pop()` 返回 `std::error_type::UNDERFLOW_ERROR`

注意 `capacity()` 返回模板容量值，但可存储元素数最多是 `Capacity - 1`。

## `std::array`

项目内 `std::array<T, N>` 是内核可用的固定长度数组实现，支持:

- `begin()`、`end()`、`cbegin()`、`cend()`
- `size()`、`max_size()`、`empty()`
- `operator[]`
- `at_nt(pos)` 无异常访问
- `front()`、`back()`、`data()`
- `fill(value)`、`swap(other)`
- `std::get<I>()`、`tuple_size`、`tuple_element`

`at()` 这类抛异常接口被标记为异常相关接口，不应在内核业务代码中使用。需要边界检查时使用 `at_nt()`。

零长度数组的 `data()` 返回 `nullptr`，`begin() == end()`。

## `std::unordered_map`

项目内 `std::unordered_map` 是链式哈希表实现，支持常见 CRUD:

- 构造、拷贝、移动、析构
- `insert()`、`emplace()`、`try_emplace()`
- `insert_or_assign()`
- `erase()`
- `operator[]`
- `find()`、`contains()`、`count()`
- `at_nt(key)` 无异常访问
- `clear()`、`swap()`、`rehash()`、`reserve()`
- `bucket_count()`、`bucket_size()`、`load_factor()`

当前实现使用桶数组加单向冲突链。源码注释指出该结构存在 pointer chasing 问题，适合写多读少或中小规模映射；读多写少的热点路径应考虑专用 flat map 或其它结构。

`max_load_factor(float)` 因浮点数在内核中不理想而被标记为 deprecated；需要设置负载因子时优先使用内部有理数形式。

## `std::ranges`

项目内 `std::ranges` 提供了内核需要的一组范围访问和算法:

- 访问: `begin`、`end`、`cbegin`、`cend`、`size`、`empty`、`data`
- 概念与类型: `range`、`input_range`、`iterator_t`、`sentinel_t`、`range_value_t`、`range_reference_t`
- 算法: `sort`、`find`、`contains`、`find_if`、`remove`、`remove_if`、`unique`、`min`、`max`、`abs`、`clamp`
- 视图: `views::all`、`views::counted`、`ref_view`、`owning_view`、`subrange`

这是标准库 ranges 的子集，覆盖当前测试与内核使用需求。新增代码如果需要未实现的 ranges 功能，应先确认是否引入新实现，而不是假设完整标准库可用。

## `util::Pair`

`util::Pair<T1, T2>` 是简单键值对结构，提供 `first`、`second` 和基础构造函数。它主要用于 `LinkedMap` 等轻量容器。若需要与标准库接口对齐，优先使用 `std::pair`。
