# FCFS Scheduler

本文总结 `kernel/schd/fcfs.h` 中的 FCFS 调度类。FCFS 是普通 FIFO 调度策略，使用 `RQ::fcfs_list` 保存 ready 线程。

## 基本属性

调度类:

```cpp
constexpr static ClassType CLASS_TYPE = ClassType::FCFS;
```

优先级低于 `RR`，高于 `IDLE`。

## 队列行为

### enqueue

`enqueue(rq, unit)` 会:

1. 取得 TCB 的 `SchedMeta`。
2. 将状态设为 `READY`。
3. 把实体插入 `rq->fcfs_list` 尾部。

### dequeue

`dequeue(rq, unit)` 会:

1. 检查实体是否在 `fcfs_list` 中。
2. 不在队列时返回 `ErrCode::INVALID_PARAM`。
3. 从队列移除。
4. 将状态设为 `EMPTY`。

### pick_next

`pick_next(rq)` 会:

1. 若队列为空，返回 `ErrCode::NO_RUNNABLE_THREAD`。
2. 取队首实体。
3. 将状态设为 `RUNNING`。
4. 从队列弹出。
5. 更新 `cursched`。
6. 返回对应 TCB。

### put_prev

`put_prev(rq, unit)` 会把当前线程设为 `READY` 并插回队尾。

## yield 与 tick

`yield()` 只给当前调度实体设置 `NEED_RESCHED`，实际切换由 `Scheduler::schedule()` 完成。

`on_tick()` 不做任何操作。FCFS 不基于时间片抢占。

## 抢占

`check_preempt_curr()` 当前固定返回 true。实际调用前，`Scheduler::check_preempt_curr()` 已保证新线程调度类优先级高于当前线程。

## 适用语义

FCFS 适合普通不需要时间片轮转的线程。当前 `TaskManager::create_task()` 允许用户请求 FCFS；fork 子线程如果父线程是 INIT，会降级为 FCFS。
