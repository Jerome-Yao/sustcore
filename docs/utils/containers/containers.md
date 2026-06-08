# Containers

本文总览 Sustcore 当前使用的容器工具。相关实现位于 `include/sus/list.h`、`include/sus/map.h`、`include/sus/ringbuf.h`、`include/sus/tree.h` 以及 `include/std/c++/` 下的标准库兼容实现。

## 分类

容器大致分为三类:

- 侵入式容器: 节点字段嵌入对象本身，容器不拥有对象生命周期。
- 普通内核容器: 容器内部通过 `new/delete` 管理节点或数组空间。
- 标准库兼容层: 提供内核可用的 `std::array`、`std::unordered_map`、`std::ranges` 等简化实现。

选择容器时首先要明确所有权:

- 如果对象生命周期由子系统管理，队列或树只表达调度/拓扑关系，应使用侵入式结构。
- 如果容器本身负责分配和释放元素节点，可使用 `LinkedList`、`ArrayList`、`LinkedMap`。
- 如果需要和 C++ 标准接口对齐，可使用项目内 `std::array`、`std::unordered_map`、`std::ranges`。

## 主要容器

| 类型 | 文件 | 用途 | 所有权语义 |
| --- | --- | --- | --- |
| `util::IntrusiveList` | `include/sus/list.h` | 调度队列、等待队列等对象关系 | 不拥有节点 |
| `util::OrderedIntrusiveList` | `include/sus/list.h` | 按比较器排序的侵入式链表 | 不拥有节点 |
| `util::LinkedList` | `include/sus/list.h` | 小规模普通双向链表 | 拥有内部节点 |
| `util::ArrayList` | `include/sus/list.h` | 动态数组列表 | 拥有数组空间 |
| `util::LinkedMap` | `include/sus/map.h` | 小规模线性映射 | 拥有内部 entry |
| `util::StaticRingBuffer` | `include/sus/ringbuf.h` | 固定容量环形队列 | 拥有静态数组空间 |
| `util::RingBuffer` | `include/sus/ringbuf.h` | 动态容量环形队列 | 拥有动态分配数组空间 |
| `util::tree::Tree` | `include/sus/tree.h` | 嵌入式树关系 | 不拥有节点 |
| `util::tree_base::TreeBase` | `include/sus/tree.h` | 继承式树关系 | 不拥有派生对象 |
| `std::array` | `include/std/c++/array` | 固定长度数组 | 值语义 |
| `std::unordered_map` | `include/std/c++/unordered_map` | 哈希映射 | 拥有节点 |
| `std::ranges` | `include/std/c++/ranges` | 范围访问与算法 | 不拥有范围 |

## 错误处理

容器层的无异常接口通常返回 `std::result_nt<T>`，错误类型是 `std::error_type`。例如:

- `ArrayList::at(index)` 返回 `OUT_OF_RANGE`
- `StaticRingBuffer::push()` / `RingBuffer::push()` 在满队列时返回 `OVERFLOW_ERROR`
- `StaticRingBuffer::pop()` / `RingBuffer::pop()` 在空队列时返回 `UNDERFLOW_ERROR`
- `std::array::at_nt()` 和 `std::unordered_map::at_nt()` 返回 `OUT_OF_RANGE`

进入内核业务层时，应转换为 `ErrCode`:

```cpp
return list.at(index)
    .transform(unwrap_ref<Node>())
    .transform_error(always(ErrCode::OUT_OF_BOUNDARY));
```

## 当前限制

- 多数容器不是线程安全的；并发访问需要调用方用中断保护、锁或调度器上下文不变量保证。
- `ArrayList`、`LinkedList`、`LinkedMap` 适合小规模管理，不应假设其性能等同成熟标准库实现。
- `StaticRingBuffer` 与 `RingBuffer` 都保留一个空槽区分满/空，因此最大可存储元素数是 `capacity() - 1`。
- `RingBuffer` 使用 allocator 显式构造/析构元素，适合非平凡类型；`StaticRingBuffer` 当前实现更适合简单可复制类型。
- 部分容器使用 `memcpy` 复制元素，代码注释中也提示 `ArrayList` 更适合 POD 或简单可复制类型。
- 侵入式容器不负责释放节点对象，节点销毁前必须从所有相关容器中移除。
- 标准库兼容层覆盖的是内核需要的子集，不是完整标准库替代品。

## 关联文档

- [链表与数组列表](list.md)
- [映射、队列与标准库兼容容器](map_queue.md)
- [树结构工具](tree.md)
