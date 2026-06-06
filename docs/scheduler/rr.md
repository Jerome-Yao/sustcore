# Round Robin Scheduler

本文总结 `kernel/schd/rr.h` 中的 RR 调度类。RR 使用 FIFO 队列和固定时间片，在 tick 中消耗时间片，耗尽后请求重新调度。

## 基本属性

调度类:

```cpp
constexpr static ClassType CLASS_TYPE = ClassType::RR;
```

时间片:

```cpp
constexpr static int TIME_SLICES = 5;
```

每个 TCB 中的 `rr_entity` 保存:

```cpp
struct Entity {
    int slice_cnt = 0;
};
```

## 队列行为

RR 使用 `RQ::rr_list`。

### enqueue

入队时:

1. 状态设为 `READY`。
2. 插入 `rr_list` 队尾。

### dequeue

出队时:

1. 要求实体位于 `rr_list`。
2. 移除实体。
3. 状态设为 `EMPTY`。

### pick_next

选择下一个线程时:

1. 队列为空返回 `ErrCode::NO_RUNNABLE_THREAD`。
2. 取队首。
3. 状态设为 `RUNNING`。
4. 重置 `slice_cnt = TIME_SLICES`。
5. 弹出队首并更新 `cursched`。

### put_prev

当前线程可继续运行但需要让出 CPU 时，`put_prev()` 会把它设为 `READY` 并插入队尾。

## tick

`on_tick()` 是 RR 的核心:

1. 若 `slice_cnt > 0`，递减。
2. 当 `slice_cnt == 0` 时，给线程设置 `NEED_RESCHED`。

下一次调度入口看到该标志后，会把当前线程放回队尾并选择下一个线程。

## yield

`yield()` 不直接切换线程，只设置当前实体 `NEED_RESCHED`。随后由 `Scheduler::yield()` 调用 `schedule()` 完成切换。

## 抢占

`check_preempt_curr()` 固定返回 true，但只有当新线程调度类优先级高于当前线程时才会被调度器调用。

## 当前限制

当前 RR 实现限制如下:

- 时间片单位是调度 tick 次数，不直接记录真实时间。
- 时间片长度固定为 5。
- 没有动态优先级或 nice 值。
