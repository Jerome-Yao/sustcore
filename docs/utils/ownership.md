# Ownership, Nonnull And RAII

本文说明 `util::owner`、`util::nonnull`、`util::Guard`、`util::UniquePtr` 以及内核回滚辅助守卫的语义。相关实现位于 `include/sus/owner.h`、`include/sus/nonnull.h`、`include/sus/raii.h` 和 `kernel/guard.h`。

## `util::owner<T *>`

`util::owner<T *>` 是所有权标注类型。它包装裸指针，明确“调用方负责释放或转移该资源”。

重要语义:

- `owner` 本身不会自动 `delete`。
- 构造函数是 `explicit`，避免裸指针隐式变成所有权。
- 可以 `get()` 取出裸指针。
- 支持 `operator*`、`operator->`、布尔判断和向裸指针转换。
- 可从可转换的 `owner<U *>` 构造。

示例:

```cpp
Result<util::owner<Device *>> create_device()
{
    auto *dev = new Device();
    if (dev == nullptr) {
        unexpect_return(ErrCode::ALLOCATION_FAILED);
    }
    return util::owner<Device *>(dev);
}
```

调用方拿到 `owner` 后必须清楚最终释放路径。常见方式包括转交给管理器、放入容器、用 `delete_guard` 回滚，或交给 `UniquePtr`。

## `util::nonnull<T *>`

`util::nonnull<T *>` 表达非空指针前置条件。构造时会断言指针非空，并禁止从 `nullptr` 构造。

构造方式:

- `util::nonnull(obj)` 从引用构造。
- `util::nnullfrom(ptr)` 从裸指针构造，失败返回 `std::result_nt<nonnull<T *>>`。
- `util::nnullforce(ptr)` 强制构造，内部断言非空。

示例:

```cpp
Result<util::nonnull<TCB *>> Scheduler::pick_next_task()
{
    auto *next = rq()->pick();
    if (next == nullptr) {
        unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
    }
    return util::nnullforce(next);
}
```

`nonnull` 不表达所有权。它只说明在当前接口契约中该指针不应为空。

## `owner` 与 `nonnull` 的区别

| 类型 | 表达内容 | 是否自动释放 | 典型用途 |
| --- | --- | --- | --- |
| `util::owner<T *>` | 拥有或转移释放责任 | 否 | 工厂返回值、容器元素、回滚对象 |
| `util::nonnull<T *>` | 指针在此处不为空 | 否 | 函数参数、查找结果、调度对象引用 |
| `T *` | 普通地址或可空引用 | 否 | 可空查询、弱引用、外部对象 |

不要用 `owner` 表达“非空”，也不要用 `nonnull` 表达“负责释放”。

## `util::Guard`

`Guard<Deleter>` 是一次性作用域守卫。析构时执行传入的清理函数，调用 `release()` 后取消清理。

典型回滚模式:

```cpp
auto obj = util::owner(new Obj());
auto obj_guard = delete_guard(obj);

propagate(register_obj(obj));

obj_guard.release();
return obj;
```

`Guard` 禁止拷贝和移动，避免作用域清理责任被隐式转移。

## `delete_guard`

`delete_guard(util::owner<T *> ptr)` 位于 `kernel/guard.h`，返回一个析构时 `delete ptr` 的 `Guard`。

它常用于多步骤构造:

1. 创建对象。
2. 建立回滚守卫。
3. 执行可能失败的注册、插入、绑定。
4. 成功后 `release()` 守卫。

注意 `delete_guard` 捕获的是所有权标注，但并不会把原变量清空。成功路径必须明确转移或释放守卫，避免双重释放或泄漏。

## `remove_guard`

`remove_guard(cholder, idx)` 位于 `kernel/guard.h`，析构时调用 `CHolder::remove(idx)` 并断言成功。它用于能力槽位插入后的失败回滚。

典型模式:

```cpp
auto insert_res = holder->insert(cap);
propagate(insert_res);

auto cap_guard = remove_guard(holder, insert_res.value());

propagate(next_step());

cap_guard.release();
void_return();
```

## `util::UniquePtr`

`UniquePtr<T, Deleter>` 是项目内的独占所有权 RAII 指针，类似简化版 `std::unique_ptr`。

支持:

- 默认构造和 `nullptr` 构造。
- 从裸指针构造。
- 从 `util::owner<U *> &&` 接管所有权，并把原 owner 置空。
- 移动构造和移动赋值。
- 跨类型移动构造。
- 自定义 deleter。
- `get()`、`operator*`、`operator->`、`operator bool`。
- `release()` 返回裸指针并放弃管理。
- `release_owner()` 返回 `util::owner<T *>`。
- `reset()` 释放旧对象并接管新对象。
- `swap()`。

`make_unique<T>(args...)` 用于创建 `UniquePtr<T>`。

当前实现不支持数组类型。

## 使用建议

- 工厂函数需要把对象交给调用方或管理器时，返回 `Result<util::owner<T *>>`。
- 局部临时对象需要自动释放时，优先用 `UniquePtr` 或 `delete_guard`。
- 多步骤提交场景优先使用 `Guard` 表达回滚点。
- 接口参数如果要求非空，用 `util::nonnull<T *>`，不要只在函数体开头写 `assert(ptr)`。
- 能力槽位插入、对象注册、调度队列入队等操作成功后，如果后续步骤仍可能失败，应立即建立回滚守卫。
