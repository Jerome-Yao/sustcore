# List Utilities

本文说明 `include/sus/list.h` 中的链表与数组列表工具。

## `ListHead<Node>`

`ListHead<Node>` 是侵入式链表节点字段:

```cpp
template <typename Node>
struct ListHead {
    Node *prev;
    Node *next;
};
```

需要加入侵入式链表的对象必须内嵌一个 `ListHead<Node>` 成员。默认成员名是 `list_head`，也可以通过模板参数指定其它成员指针。

## `IntrusiveList`

`IntrusiveList<Node, Head>` 是双向循环哨兵链表。容器内部有一个哨兵节点和元素计数。

典型定义:

```cpp
struct Waiter {
    util::ListHead<Waiter> list_head;
    tid_t tid;
};

using WaitQueue = util::IntrusiveList<Waiter>;
```

核心接口:

- `begin()`、`end()`、`cbegin()`、`cend()`
- `front()`、`back()`
- `insert(pos, node)`
- `erase(pos)`
- `remove(node)`
- `push_front(node)`、`push_back(node)`
- `pop_front()`、`pop_back()`
- `clear()`
- `contains(node)`

### 不变量

- 同一节点不能重复插入同一个或其它同类型链表。实现通过检查节点 `prev/next` 是否为空判断是否已链接。
- `erase()` 会把节点的 `prev/next` 清空，使该节点可以重新插入。
- `clear()` 只解除链表关系，不销毁节点对象。
- 节点对象释放前必须先从链表中移除。

## `OrderedIntrusiveList`

`OrderedIntrusiveList<Node, Head, Cmp>` 在 `IntrusiveList` 上增加有序插入:

```cpp
util::OrderedIntrusiveList<Task, &Task::runqueue_head, TaskLess> rq;
rq.insert(task);
```

它删除了任意位置插入和头尾插入接口，只允许通过比较器 `Cmp` 决定插入位置。适合调度类、定时器队列等需要按优先级或时间排序的场景。

## `LinkedList`

`LinkedList<T>` 是普通双向链表，容器内部为每个元素分配节点，析构或 `clear()` 时释放节点。

核心接口:

- `push_front(value)`、`push_back(value)`
- `insert(pos, value)`
- `erase(pos)`
- `remove(value)`
- `pop_front()`、`pop_back()`
- `front()`、`back()`
- `contains(value)`
- 双向迭代器

与侵入式链表不同，`LinkedList<T>` 拥有内部节点。但它仍不是线程安全容器，也不适合频繁大规模分配场景。

## `ArrayList`

`ArrayList<T>` 是动态数组列表，默认容量为 8，容量不足时扩容。它支持随机访问和迭代。

核心接口:

- `size()`、`empty()`、`capacity()`
- `reserve(new_capacity)`、`shrink_to_fit()`
- `operator[](index)`
- `at(index)` 返回 `std::result_nt<T &>`
- `front()`、`back()`
- `insert()`、`erase()`
- `push_front()`、`push_back()`、`emplace_front()`、`emplace_back()`
- `pop_front()`、`pop_back()`
- `clear()`、`find()`、`contains()`

### 使用约束

`ArrayList` 的实现使用 `new[]` 分配数组，并在复制、扩容时使用 `memcpy`。因此它更适合 POD 或简单可复制类型。对于带复杂所有权、析构副作用或移动语义要求严格的类型，应谨慎使用，必要时优先使用标准容器兼容层或明确实现专用结构。

### 错误处理

`at(index)` 越界时返回:

```cpp
std::error_type::OUT_OF_RANGE
```

业务层应转换为 `ErrCode`:

```cpp
return items.at(index)
    .transform(unwrap_ref<Item>())
    .transform_error(always(ErrCode::OUT_OF_BOUNDARY));
```

## 选择建议

- 调度单元、等待队列、对象生命周期由外部管理: 使用 `IntrusiveList`。
- 需要保持顺序并按比较器插入: 使用 `OrderedIntrusiveList`。
- 小规模、容器自持节点的线性集合: 使用 `LinkedList`。
- 小规模随机访问集合: 使用 `ArrayList`。
- 需要哈希查找: 使用 `std::unordered_map` 或专门映射结构。
