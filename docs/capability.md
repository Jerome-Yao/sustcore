Capability 系统

概述
- Capability 是内核对象的统一访问令牌. 每个 capability 由一个 payload
	指针和权限位图构成.
- 统一 ABI 定义在 include/sustcore/capability.h, 内核/用户态共用.
- 核心运行时 (payload, capability, CSpace, CHolder) 实现在 kernel/cap.
- 对象级行为由 kernel/object 中的 CapObj 子类封装.

统一接口 (内核/用户态共享)
- `PayloadType` (include/sustcore/capability.h)
	- NONE, INTOBJ, SINTOBJ, VFILE, NOTIF, MUTEX, PCB, TCB, ENDPOINT, MEMORY,
		REPLY.
	- 作为 payload 的运行时类型标识.
- `CapIdx` / `RecvIdx`
	- `CapIdx` 为 64 位索引, 布局: VALID 位 + GROUP + SLOT.
	- 辅助函数: `cap::make(group, slot)`, `cap::valid(idx)`,
		`cap::group(idx)`, `cap::slot(idx)`.
	- `CSPACE_SIZE`, `CGROUP_SLOTS`, `CSPACE_CAPACITY` 由掩码位宽计算得出.
- `CapInfo`
	- { type, permissions } 用于 capability 查询结果回写.

核心运行时
`Payload` (kernel/cap/capability.h)
- 所有能力载荷的基类, 内置引用计数.
- `clone_payload()` 默认返回自身 (共享载荷). 需要深拷贝语义时重写.
- `destruct()` 默认 delete, 可由对象重写定制释放逻辑.

`Capability` (kernel/cap/capability.h)
- 保存 `Payload*` 与权限位.
- 构造时 keep payload, 析构时 release.
- 拷贝构造通过 `clone_payload()` 生成新 payload.
- `downgrade(new_perm)` 先用 `perm::imply` 校验再降权.
- `imply(required)` 判定权限包含关系.
- 使用 KOP 内核对象池分配 (kernel/cap/capability.cpp), 由 `init_kop()`
	初始化.

`CapObj<TPayload>`
- 对 `Capability*` + payload 的轻量封装.
- 集中权限校验并提供对象操作入口.

CSpace 结构
`CGroup`
- 固定大小 `CGROUP_SLOTS` 的 capability 数组.
- `get` / `set` / `take` / `remove` 管理槽位.
- 使用 KOP 内核对象池分配.

`CSpace`
- `CGroup*` 数组, 按需分配 group.
- `get` / `set` / `take` / `remove` / `move` 按 `CapIdx` 管理 capability.
- `lookup_freeslot()` 找到第一个空闲槽位.
- `foreach()` 迭代全部非空 capability.
- `clear()` 释放所有 group.

`CHolder` (Capability Holder)
- 每个持有者包含一个 `CSpace` 和唯一 id.
- `lookup` / `access`: 按 `CapIdx` 获取 capability.
- `insert` / `insert_to_free`: 插入 payload 并设置权限.
- `create<TPayload>(args...)`: 构造 payload 并放入首个空闲槽位.
- `remove` / `clear`: 删除单个或清空全部 capability.
- `clone`: 复制 capability 到新槽位.
	- 若 payload 为 `MemoryPayload` 且非 shared, 会调用
		`TaskMemoryManager::protect_memory_cow` 建立 COW.
- `derive`: 先 clone 再 `downgrade`.
- `downgrade`: 原地降权.
- `transfer_to`: 将 capability 传递到另一个 holder.
	- 若源 capability 含 `perm::basic::CLONE`, 目标得到 clone.
	- 若含 `MIGRATE` 或 `MIGRATE_ONCE`, 目标得到相同 payload 与权限, 且
		`MIGRATE_ONCE` 会在目标能力中清除.
	- 若两者皆无, 传递被拒绝.
- `copy_all_to`: 按相同槽位复制全部 capability.

`CHolderManager`
- 单例管理所有 `CHolder`.
- 提供 `create_holder` / `remove_holder` / `get_holder`.
- `timestamp()` 返回递增计数, 用于 IPC 等路径的时间戳.

权限模型
通用 helpers (kernel/cap/permission.h)
- `perm::imply(owned, required)` 按位包含判断.
- `perm::allperm()` 返回全 1 权限位.

基础权限 (kernel/object/perm.h)
- `perm::basic::CLONE`
- `perm::basic::MIGRATE`
- `perm::basic::MIGRATE_ONCE`

对象权限 (kernel/object/perm.h)
- `perm::endpoint`: GRANT, READ, WRITE
- `perm::reply`: CALLER, REPLIER
- `perm::intobj`: READ, WRITE, INCREASE, DECREASE
- `perm::sintobj`: READ, WRITE, INCREASE, DECREASE
- `perm::memory`: MAP, READ, WRITE, EXEC, RESIZE, QUERY, FLEXUP, FLEXDOWN
- `perm::mutex`: USE, QUERY, DESTROY
- `perm::notif`: 从 bit16 起, 每个信号占用 2 位 (SIGNAL/QUERY)
- `perm::pcb`: GETPID, KILL, VMCONTEXT, NEW_THREAD, NEW_PROCESS, EXECUTE
- `perm::vfile`: READ, WRITE, EXEC

能力对象 (Payload + CapObj 封装)

INTOBJ
- Payload: `IntPayload { value }`.
- Object: `IntObj`.
- 操作: read, write, increase, decrease.
- 权限: read/write 使用 `perm::intobj`; increase/decrease 按实现使用
	`perm::sintobj` 位.

SINTOBJ
- `PayloadType::SINTOBJ` 与 `perm::sintobj` 已定义.
- 当前 kernel/object 中未提供对应 payload 或 CapObj 实现.

VFILE
- Payload: `VFile` (kernel/vfs/vfs.h), 持有 `VINode` 引用.
- 生命周期: `destruct()` 标记 discarded, 析构时若未 discarded 会日志告警.
- 权限: `perm::vfile::READ/WRITE/EXEC`.

NOTIF (Notification)
- Payload: `NotificationPayload` 维护 `signalbits` 与每个信号的等待原因.
- Object: `NotificationObject`.
- 操作:
	- signal / unsignal / set: 更新信号并唤醒等待者.
	- query: 查询信号状态.
	- wait: 阻塞等待信号.
- 权限: 每个信号独立校验 `SIGNAL/QUERY` 位.

MUTEX
- Payload: `MutexPayload { locked }`.
- Object: `MutexObject`.
- 操作: lock / unlock (已锁/未锁时返回 false).
- 权限: `perm::mutex::USE` (QUERY/DESTROY 预留).

ENDPOINT
- Payload: `EndpointPayload`, 含消息队列与 send/recv 等待队列.
- Object: `EndpointObject`.
- 操作:
	- `send_async`: 异步入队并唤醒接收方, 需 WRITE.
	- `send_sync`: 入队并等待被消费, 需 WRITE.
	- `recv_async`: 出队或返回 nullptr, 需 READ.
	- `recv_sync`: 等待消息可用, 需 READ.
	- `call`: 生成 Reply 能力对, 将 replier cap 附在请求中, 等待回复.
- 权限:
	- 接收需 READ, 发送需 WRITE, 携带 cap 需 GRANT.

REPLY
- Payload: `ReplyPayload`, 最多持有一条回复消息.
- Object: `ReplyObject`.
- 操作:
	- `send_reply`: 写入一次回复, 需 REPLIER.
	- `reply`: 写入回复并从 holder 中移除 reply cap.
	- `recv_async` / `recv_sync`: 读取回复, 需 CALLER.
- 权限: `perm::reply::CALLER` / `perm::reply::REPLIER`.

MEMORY
- Payload: `MemoryPayload`
	- memsz: 承诺大小
	- shared: true 时 clone 返回自身 payload
	- continuity: 要求物理连续
	- growth: `MemoryGrowth` (FIXED/FLEXUP/FLEXDOWN)
	- phy_pages: 已分配物理页列表 (懒分配)
- 关键操作:
	- `lookup_page` / `ensure_page` / `replace_page`
	- `resize` (按 growth 约束, continuity 需重分配并拷贝)
	- `release_pages_from`, `allocated_size`
	- `clone_payload`: shared 直接共享; 非 shared 复制并通过 GFP keep
		进入 COW.
- Object: `MemoryObject`
	- `map_into`: 校验 MAP + READ/WRITE/EXEC + FLEXUP/FLEXDOWN, 创建 VMA.
	- `unmap_from`: 校验 MAP, 移除 VMA.
	- `resize_in`: 校验 RESIZE 并同步 VMA.
	- `query`: 校验 QUERY, 返回 memsz 与 allocated.

PCB / TCB
- Payload:
	- `PCBPayload` 持有 `task::PCB*`, `destruct()` 会将 PCB 入回收队列.
	- `TCBPayload` 持有 `task::TCB*`.
- Object: `PCBObject`
	- `get_pid`: GETPID.
	- `kill`: KILL, 标记退出并清理 holder, 必要时触发调度.
	- `map`: VMCONTEXT, 继续调用 `MemoryObject::map_into`.
	- `require_new_thread` / `require_new_process` / `require_execute` /
		`require_new_process_execute`: 任务创建/执行权限门控.
- Object: `TCBObject` 为类型封装, 未添加额外操作.

系统调用层 (kernel/syscall/*)
Capability syscall (kernel/syscall/cap.*)
- `cap_clone`, `cap_derive`, `cap_downgrade`, `cap_remove`.
- `sys_cap_lookup` 回写 `CapInfo`; 空槽位返回 false.
- `cap_remove` 若 Memory 仍被映射则返回 BUSY.

Endpoint syscall (kernel/syscall/endpoint.*)
- `endpoint_create` 创建 endpoint 并插入当前 holder.
- `endpoint_send_sync` / `endpoint_send_async`.
- `endpoint_recv_sync` / `endpoint_recv_async`.
- `endpoint_call` 自动创建 reply cap, 发送后等待回复并清理.
- `endpoint_reply` 写回复并移除 reply cap.

Notification syscall (kernel/syscall/notif.*)
- `notification_create`.
- `notification_signal` / `notification_unsignal` / `check_notification`.
- `wait_notification`.

Memory syscall (kernel/syscall/memory.*)
- `mem_create`, `mem_unmap`, `mem_resize`, `mem_query`.
- `mem_create` 不允许 shared 且 growth 非 FIXED.

Task syscall (kernel/syscall/task.*)
- `pcb_create_process`: 按列表复制 capability (要求 CLONE), 加载 ELF
	并返回子进程 PCB cap.
- `pcb_create_thread`: 要求 NEW_THREAD.
- `pcb_fork`: fork 当前进程并返回子 PCB cap.
- `pcb_kill`, `pcb_map`, `pcb_execve`, `pcb_is_current`, `get_pid`.

用户态接口 (include/kmod/syscall.h)
- 预置 capability: `__pcb_cap`, `__main_tcb_cap`, `__heap_mem_cap`,
	`__stack_mem_cap`.
- Capability 操作: `sys_cap_clone`, `sys_cap_derive`, `sys_cap_downgrade`,
	`sys_cap_remove`, `sys_cap_lookup`.
- Notification: `sys_notif_create`, `sys_notif_signal`, `sys_notif_unsignal`,
	`sys_notif_check`, `sys_notif_wait`.
- Endpoint: `sys_endpoint_create`, `sys_endpoint_send/recv`(sync),
	`sys_endpoint_send_async/recv_async`, `endpoint_call`, `endpoint_reply`.
- Memory: `sys_mem_create`, `sys_mem_map`, `sys_mem_unmap`, `sys_mem_resize`,
	`sys_mem_query`.
- Task: `sys_pcb_create_process`, `sys_create_process`, `sys_pcb_create_thread`,
	`sys_create_thread`, `sys_pcb_fork`, `fork`, `sys_pcb_execve`, `sys_execve`,
	`execve`, `sys_pcb_kill`, `sys_pcb_map`, `sys_getpid`.
- 说明: 内核 syscall 枚举中当前仅提供 `SYS_PCB_MAP` 作为映射入口, 未
	单独定义 `SYS_MEM_MAP`.
