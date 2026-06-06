# INIT Scheduler

本文总结 `kernel/schd/init.h` 中的 INIT 调度类。INIT 是系统启动阶段的特殊调度类，用于承载 init 进程主线程。

## 基本属性

调度类:

```cpp
constexpr static ClassType CLASS_TYPE = ClassType::INIT;
```

优先级高于 `RR`，低于 `RT`。

INIT 与 IDLE 类似，是单槽调度类:

```cpp
SchedMeta *ready = nullptr;
```

## 队列行为

### enqueue

将线程设为 `READY`，并记录到 `ready`。

### dequeue

要求目标实体等于 `ready`，然后清空 ready 并设为 `EMPTY`。

### pick_next

若 ready 存在，则取出该线程，状态设为 `RUNNING`，并更新 `cursched`。

### put_prev

把当前 init 线程重新设为 `READY` 并记录到 ready 槽。

## tick 与 yield

`on_tick()` 不做操作。INIT 不使用时间片。

`yield()` 会设置当前实体 `NEED_RESCHED`。

## 创建路径

`TaskManager::create_init_task()` 固定使用:

```cpp
constexpr schd::ClassType INIT_SCHED_CLASS = schd::ClassType::INIT;
```

普通用户进程不能通过 `create_task()` 请求 INIT 调度类。fork 时如果父线程属于 INIT，子线程会降级为 FCFS，避免普通子进程继续占用 INIT 类。

## 当前限制

INIT 当前是单槽模型，没有多个 init 类线程的队列语义。它更像启动阶段的特殊优先级，而不是通用高优先级调度策略。
