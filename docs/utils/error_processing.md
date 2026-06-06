# Error Processing

本文总结 Sustcore 当前的错误处理模型、统一错误码、`Result<T>` 类型以及常用简化函数/宏。相关实现主要位于 `include/sustcore/errcode.h`、`include/std/c++/expected`、`include/std/c++/nt/errors.h`。

## 总体模型

内核禁止使用 C++ 异常。可恢复失败统一通过 `Result<T>` 返回:

```cpp
template <typename T>
using Result = std::result<T, ErrCode>;
```

其中 `std::result<T, E>` 是项目对 `std::expected<T, E>` 的别名增强:

- 普通 `T` 映射为 `std::expected<T, E>`
- `T &` 映射为 `std::expected<std::reference_wrapper<T>, E>`
- `const T &` 映射为 `std::expected<std::reference_wrapper<const T>, E>`

因此返回引用结果时通常需要配合 `unwrap_ref<T>()` 还原引用。

## `ErrCode`

`ErrCode` 使用高位区分错误域，低位表示域内错误:

- 通用错误: `INVALID_PARAM`、`OUT_OF_BOUNDARY`、`NOT_SUPPORTED`、`BUSY`、`OUT_OF_MEMORY`、`NULLPTR`、`ALLOCATION_FAILED`、`KEY_DUPLICATED`
- 能力错误: `INVALID_CAPABILITY`、`INSUFFICIENT_PERMISSIONS`、`TYPE_NOT_MATCHED`、`PAYLOAD_ERROR`、`CREATION_FAILED`、`INVALID_TOKEN`、`NO_FREE_SLOT`
- 文件系统错误: `ENTRY_NOT_FOUND`
- 任务错误: `NO_RUNNABLE_THREAD`、`NO_MESSAGE`
- 内存错误: `PAGE_NOT_PRESENT`、`INVALID_PTE`
- 设备错误: `FDT_ERROR`、`INVALID_PROPERTY_TYPE`

`to_cstring(ErrCode)` 用于把错误码转换为稳定的字符串，适合日志输出。

## 基本返回宏

`include/sustcore/errcode.h` 提供了几类短宏，目的是减少重复的 `if (!res) return ...`。

```cpp
#define unexpect_return(x)  return std::unexpected(x)
#define propagate_return(x) unexpect_return(x.error())
#define propagate(x)        ...
#define co_propagate(x)     ...
#define void_return()       return std::expected<void, ErrCode>{}
```

推荐写法:

```cpp
Result<void> setup()
{
    auto init_res = init_device();
    propagate(init_res);

    auto bind_res = bind_irq();
    propagate(bind_res);

    void_return();
}
```

`propagate(x)` 用于普通函数；`co_propagate(x)` 用于返回 `util::cotask<Result<T>>` 的协程函数。二者都要求 `x` 是可访问 `has_value()` 和 `error()` 的结果对象。

## 成功路径与失败路径

推荐结构是:

1. 入口先做参数、权限、状态检查。
2. 对每个可能失败的步骤调用 `propagate(...)` 或使用链式处理。
3. 成功路径保持直线，不在深层嵌套中堆叠业务逻辑。

示例:

```cpp
Result<size_t> find_slot(CHolder &holder, CapIdx idx)
{
    if (!cap::valid(idx)) {
        unexpect_return(ErrCode::INVALID_CAPABILITY);
    }

    auto cap_res = holder.lookup(idx);
    propagate(cap_res);

    return cap::slot(idx);
}
```

当需要先清理再返回错误时，不要直接 `propagate(...)`，应显式保存错误码或使用作用域守卫保证清理完成。

## 链式处理

项目内 `std::expected` 支持常用单子式接口:

- `.and_then(f)`: 只在成功时调用 `f`，`f` 必须返回同错误类型的 `expected`。
- `.transform(f)`: 只在成功时映射值，失败时透传错误。
- `.or_else(f)`: 只在失败时调用 `f`，可用于恢复或统一错误处理。
- `.transform_error(f)`: 只映射错误值。
- `.filter(pred, err)`: 成功值不满足谓词时转为指定错误。
- `.discard_value()`: 丢弃成功值，转成 `expected<void, E>`。

常见用途:

```cpp
return table.at_nt(id)
    .transform(unwrap_ref<Entry *>())
    .transform_error(always(ErrCode::OUT_OF_BOUNDARY));
```

这个写法表达了三件事:

- `at_nt` 返回标准库无异常错误类型。
- `unwrap_ref` 把 `reference_wrapper` 拆成原始值。
- `transform_error` 把局部错误码转换成内核统一 `ErrCode`。

## 辅助函数

`always(value)` 返回一个忽略参数并返回固定值的函数对象，通常用于错误码或固定成功值映射:

```cpp
return wake_res.transform(always(true));
```

`unwrap_ref<T>()` 返回 `std::reference_wrapper<T>::get` 的函数对象，用于 `std::result<T &, E>` 或 `std::result<const T &, E>`。

`unwrap_owner<T>()` 返回 `util::owner<T>::get` 的函数对象，用于从所有权标注中取出裸指针。

`this_call(self, func)` 把成员函数绑定到对象指针，适合传给链式接口或算法。

`pred::equal_to(value)` 生成等值谓词，常见于 `.filter(...)`。

## `Optional<T>`

`include/sustcore/errcode.h` 还定义了:

```cpp
template <typename T>
using Optional = ...
```

它对引用类型做了特殊处理:

- `Optional<T>` 是 `std::optional<T>`
- `Optional<T &>` 是 `std::optional<std::reference_wrapper<T>>`
- `Optional<const T &>` 是 `std::optional<std::reference_wrapper<const T>>`

当“没有结果”不是错误时使用 `Optional<T>`；当调用方需要知道失败原因时使用 `Result<T>`。

## 标准库无异常结果类型

`include/std/c++/nt/errors.h` 定义了 `std::error_type` 和 `std::result_nt<T>`，用于标准库兼容层内部:

- `OVERFLOW_ERROR`
- `UNDERFLOW_ERROR`
- `OUT_OF_RANGE`
- `NULLPTR`

例如 `std::array::at_nt()`、`std::unordered_map::at_nt()`、`util::ArrayList::at()` 等返回这类错误。进入内核业务层后，通常应通过 `.transform_error(...)` 转换为 `ErrCode`。

## 注意事项

- 不要在内核业务代码中依赖 `expected::value()` 抛异常路径；只有在已经通过 `propagate`、`tassert` 或不变量证明成功时才取值。
- `std::expected` 文件中保留了符合标准接口的异常相关类型，但项目约定禁止通过异常做错误处理。
- 宏参数会被求值，传入表达式时应避免有副作用的复杂临时表达式。
- `propagate(x)` 只传播错误，不记录日志；需要上下文时应在返回前用模块日志记录关键参数和错误码。
