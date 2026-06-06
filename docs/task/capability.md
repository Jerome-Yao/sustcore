# Task And Capability

本文总结任务系统与能力系统的交互。Sustcore 中进程资源不是靠全局句柄直接暴露给用户态，而是通过 `CHolder` 中的 capability 访问。PCB、TCB、Memory、VFile 等对象都以 payload 形式进入进程 capability 空间。

## 进程 capability 空间

每个用户 `PCB` 持有:

```cpp
cap::CHolder *cholder;
```

该 holder 是进程的 capability space。任务系统创建进程时会把以下能力插入 holder:

- ELF image file capability。
- PCB 自身 capability。
- heap memory capability。
- stack memory capability。
- 主 TCB capability。

进程 fork、exec、创建线程、映射内存都围绕这个 holder 工作。

## PCB payload

`cap::PCBPayload` 持有内核 `task::PCB *`。

最后一个 PCB capability 被释放时:

1. `PCBPayload::destruct()` 被调用。
2. payload 调用 `TaskManager::enqueue_recycle(pcb)`。
3. PCB 被加入延迟回收队列。
4. 下一次 trap 入口会触发 `reap_recycled()` 完成真实释放。

这避免在 capability 删除路径中立即销毁仍可能被调度器或 trap 路径引用的 PCB。

## TCB payload

`cap::TCBPayload` 持有内核 `task::TCB *`。

当前 TCB capability 主要用于把线程身份暴露给用户启动信息和后续扩展。真正的线程创建和唤醒仍由 `TaskManager` 与 `Scheduler` 完成。

## PCBObject 权限

`cap::PCBObject` 集中处理进程相关权限检查。

主要方法与权限:

- `get_pid()` 需要 `perm::pcb::GETPID`。
- `kill()` 需要 `perm::pcb::KILL`。
- `map()` 需要 `perm::pcb::VMCONTEXT`。
- `require_new_thread()` 需要 `NEW_THREAD`。
- `require_new_process()` 需要 `NEW_PROCESS`。
- `require_execute()` 需要 `EXECUTE`。
- `require_new_process_execute()` 需要 `NEW_PROCESS | EXECUTE`。

syscall 层只负责 lookup capability 和构造 `PCBObject`，具体权限由对象方法完成。

## 进程创建

`SYS_CREATE_PROCESS` 的 syscall 路径会:

1. 通过传入的 PCB capability lookup 目标 PCB。
2. 构造 `PCBObject`。
3. 要求 `NEW_PROCESS | EXECUTE` 权限。
4. 从父进程 holder 中按用户提供的 cap 列表复制初始 capability。
5. 使用配置好的子 holder 加载 ELF。
6. 把子进程 PCB capability 再插入当前 holder，返回给调用者。

复制初始 capability 时要求源 capability 具备 `perm::basic::CLONE`。

对于非 shared Memory capability，还会调用父 TMM 的 `protect_memory_cow(memory)`，把父进程现有映射转成 COW 写保护。

## fork

fork 会复制整个父 holder:

1. 遍历父进程 capability space。
2. 对 Memory capability，优先查找 fork 时已经克隆到子 TMM 的对应 memory payload。
3. 如果找不到对应克隆 memory，则调用 `clone_payload()`。
4. 用同样的 CapIdx 和权限插入子 holder。
5. 在父子 holder 的同一 `ret_slot` 插入子 PCB payload。

这保证 fork 后父子 capability 索引尽量保持一致。Memory payload 与页表映射同时进入 COW 状态。

## exec

exec 的 capability 语义由 reserved cap 列表控制。

执行前:

- 验证 `pcb_cap` 存在。
- 验证每个 reserved cap 存在。
- 加载新镜像需要临时插入 image file capability。

替换镜像时:

1. 移除 holder 中除 PCB capability 和 reserved caps 之外的所有 capability。
2. 创建新的 heap/stack memory capability。
3. 创建新的主 TCB capability。
4. 保留同一个 PCB capability，使进程身份延续。

因此 exec 后地址空间被替换，但允许显式保留部分跨 exec capability。

## Memory capability 与地址空间

Memory capability 通过 `MemoryObject::map_into()` 映射到目标 PCB 的 TMM。

权限检查分两层:

1. `PCBObject::map()` 检查目标 PCB capability 的 `VMCONTEXT`。
2. `MemoryObject::map_into()` 检查 Memory capability 的 `MAP`、`READ`、`WRITE`、`EXEC`、`FLEXUP`、`FLEXDOWN`。

取消映射、resize、query 由 memory syscall 和 `MemoryObject` 继续负责。

## kill 与回收

`PCBObject::kill()` 会:

1. 检查 `KILL` 权限。
2. 设置 PCB 退出状态。
3. 清空 holder。
4. 将线程标为 WAITING。
5. 对非当前 READY 线程执行 dequeue。
6. 当前线程杀死自己时触发调度。

holder 被清空后，PCB payload 生命周期结束会推动延迟回收。

## 当前限制

当前 task/capability 交互仍有一些限制:

- TCB capability 操作对象目前能力较少。
- exec 的失败回滚仍不完整。
- fork 复制 capability 时没有单独的 per-type 深层策略表，主要依赖 payload 的 `clone_payload()`。
- 内核 PCB 没有 holder，不参与用户 capability 语义。
