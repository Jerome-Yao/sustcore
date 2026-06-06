# Coroutine Utilities

本文说明 `include/sus/coroutine.h` 中的 `util::cotask<T>` 协程任务类型，以及它和 `Result<T>` 错误传播的配合方式。

## 基本模型

`util::cotask<T>` 是一个轻量协程返回对象，配套 promise 类型为 `util::promise<T>`。

支持两类返回值:

- `util::cotask<T>`
- `util::cotask<void>`

promise 的 `initial_suspend()` 返回 `std::suspend_never`，因此协程创建后会立即开始执行，直到完成或遇到真正的挂起点。

## 生命周期

`cotask` 持有协程 handle，并在析构时销毁仍由它拥有的协程帧。

核心状态:

- `_handle`: 协程句柄。
- `_owns_handle`: 当前 `cotask` 是否负责销毁协程帧。
- `promise.detached`: 是否脱离 `cotask` 所有权。
- `promise.destroy_on_final_suspend`: 是否在最终挂起时销毁帧。
- `promise.continuation`: `co_await` 时需要恢复的 continuation。

当前语义区分两类任务:

- **普通任务**: `cotask` 对象拥有协程帧，可 `co_await`、读取结果，并在析构时负责销毁帧
- **detached 任务**: 调用 `detach()` 后转为 fire-and-forget，对象立即失效，不再允许任何交互

## 核心接口

`cotask<T>` 提供:

- `valid()`: 是否有有效 handle。
- `done()`: 无 handle 或协程已经完成。
- `result()`: 读取 promise 中保存的返回值；无 handle 时返回默认值。
- `resume()`: 如果未完成则恢复协程。
- `handle()`: 返回原始协程句柄。
- `detach()`: 放弃当前对象对协程帧的所有权。
- `operator co_await()`: 支持被其它协程等待。

`cotask<void>` 没有 `result()`，其它生命周期接口一致。

## `co_await` 行为

`cotask` 的 awaiter 在被等待时:

1. 如果任务为空或已完成，`await_ready()` 返回 true，同步继续。
2. 如果任务未完成，`await_suspend()` 记录 continuation。
3. 子任务最终挂起时优先恢复 continuation。
4. `await_resume()` 返回结果。

测试覆盖普通等待与已完成任务 detach 释放帧等路径。

## 与 `Result<T>` 配合

内核异步 syscall 使用 `util::cotask<Result<T>>` 风格。普通错误传播宏不能在协程中直接 `return`，应使用 `co_propagate(...)`。

示例:

```cpp
util::cotask<Result<void>> send_async(...)
{
    auto msg_res = build_message();
    co_propagate(msg_res);

    auto send_res = co_await endpoint_send(msg_res.value());
    co_propagate(send_res);

    co_return std::expected<void, ErrCode>{};
}
```

`co_propagate(x)` 在失败时执行:

```cpp
co_return std::unexpected((x).error());
```

## 注意事项

- `cotask` 禁止拷贝，只允许移动。
- `detach()` 后当前对象立刻失效:
  - 不再允许 `co_await`
  - 不再允许 `result()`
  - 不再允许保留 awaiter 后手动取值
- 若 `detach()` 时任务已经完成，会立即销毁帧并清空 handle。
- 若 `detach()` 时任务尚未完成，任务会在 `final_suspend` 中自我销毁，外部无需干预。
- `result()` 返回保存值的拷贝；对大型对象或所有权对象应注意移动和生命周期。
- promise 的 `unhandled_exception()` 只写入默认值或空操作；内核代码不应依赖异常路径表达错误。
- 协程函数中使用 `Result<T>` 时，失败路径必须通过 `co_return std::unexpected(...)` 或 `co_propagate(...)`。
