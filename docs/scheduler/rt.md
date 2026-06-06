# RT Scheduler

本文总结 `kernel/schd/rt.h` 中的 RT 调度类。当前 RT 是最高优先级 FIFO 调度类，继承 FCFS 的大部分行为，但使用独立的 `RQ::rt_list`。

## 基本属性

调度类:

```cpp
constexpr static ClassType CLASS_TYPE = ClassType::RT;
```

优先级高于所有其它调度类。

## 队列行为

RT 使用 `rq->rt_list`:

- `enqueue()` 将线程设为 `READY` 并插入队尾。
- `dequeue()` 要求线程在 `rt_list` 中，移除后设为 `EMPTY`。
- `pick_next()` 取队首线程，设为 `RUNNING`，弹出队首并更新 `cursched`。
- `put_prev()` 把当前线程设为 `READY` 并插回队尾。

## 与 FCFS 的关系

`RT<T>` 继承 `fcfs::FCFS<T>`，但覆写了队列相关接口，让 RT 线程使用 `rt_list` 而不是 `fcfs_list`。

未覆写的行为沿用 FCFS:

- `yield()`
- `on_tick()`
- `check_preempt_curr()`

因此当前 RT 没有 deadline、优先级队列或时间预算；它只是最高优先级 FIFO。

## 抢占语义

当 RT 线程被唤醒，而当前线程不是 RT 时，调度器会认为新线程调度类优先级更高，并调用 RT 的 `check_preempt_curr()`。该函数继承 FCFS 行为，返回 true。

## 当前限制

当前 RT 调度类只是占位式实时类:

- 没有 per-thread 实时优先级。
- 没有 deadline。
- 没有运行时间预算。
- 同级 RT 线程按 FIFO 轮转，tick 不会强制时间片切换。
