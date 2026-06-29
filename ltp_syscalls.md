# LTP Syscalls 分类结论

## 说明

本文针对两个输入做分类：

- 二进制：`.vscode/reference/rv/glibc/ltp/testcases/bin`
- 源码：`.vscode/testsuits-for-oskernel/ltp-full-20240524/testcases`

分类口径基于当前 `module/linux-subsystem/main.cpp` 中已经接入的 Linux syscall，以及现有 `kernel/vfs` / `kernel/task` / `kernel/mem` 能力。  
这里的“可以执行”指 **在当前系统调用面上具备直接运行和调试价值**，不等于所有子用例一定已经通过。  
这里的“轻度修改”指 **主要补 errno、flag、字段、边界语义**，不引入新的大机制。  
这里的“引入一整套机制”指 **不是补几个 syscall 就能完成**，而是要补一整层子系统。

另外，下面默认把大量 `.sh`、network stress、controllers、security、fs stress 类脚本测试视为“非当前阶段优先目标”，因为它们通常依赖：

- shell 工具链完整性
- 网络协议栈
- namespace / cgroup
- 完整 mount / fs 管理
- 额外内核特性而不只是 syscall 入口

## 一、当前就可以执行的测试

这类测试与当前 Linux subsystem 已实现 syscall 面匹配度最高，应优先纳入 `ltp.h`。

### 1. 基础进程 / 时间 /随机数

- `uname01`, `uname02`, `uname04`, `newuname01`
- `getrandom01` 到 `getrandom05`
- `sched_yield01`
- `gettimeofday01`, `gettimeofday02`
- `clock_gettime01` 到 `clock_gettime04`
- `nanosleep01`, `nanosleep02`, `nanosleep04`
- `times01`, `times03`
- `getpid01`, `getpid02`, `getppid01`, `getppid02`, `gettid01`, `gettid02`
- `brk01`, `brk02`, `sbrk01`, `sbrk02`, `sbrk03`

### 2. 基础文件读写 / 目录 / 路径

- `close01`, `close02`
- `dup01` 到 `dup07`
- `dup201` 到 `dup207`
- `dup3_01`, `dup3_02`
- `read01` 到 `read04`
- `write01` 到 `write06`
- `readv01`, `readv02`
- `writev01`, `writev02`, `writev03`, `writev05`, `writev06`, `writev07`
- `lseek01`, `lseek02`, `lseek07`, `lseek11`
- `getcwd01` 到 `getcwd04`
- `chdir01`, `chdir04`
- `getdents01`, `getdents02`
- `readlink01`, `readlink03`, `readlinkat01`, `readlinkat02`
- `mkdir02`, `mkdir03`, `mkdir04`, `mkdir05`, `mkdir09`
- `mkdirat01`, `mkdirat02`
- `unlink05`, `unlink07`, `unlink08`, `unlink09`, `unlinkat01`
- `rename01` 到 `rename14`
- `renameat01`, `renameat201`, `renameat202`
- `open01`, `open02`, `open03`, `open04`, `open06`, `open07`, `open08`, `open09`, `open10`, `open11`, `open12`, `open13`, `open14`
- `creat01`, `creat03`, `creat04`, `creat05`, `creat06`, `creat07`, `creat08`, `creat09`
- `openat01`, `openat02`, `openat03`, `openat04`, `openat201`, `openat202`, `openat203`

### 3. 元数据 / stat / truncate / mmap 基础路径

- `fstat02`, `fstat03`
- `stat01`, `stat02`, `stat03`
- `lstat01`, `lstat02`
- `fstatat01`
- `statx01`, `statx02`, `statx03`
- `ftruncate01`, `ftruncate03`, `ftruncate04`
- `truncate02`, `truncate03`
- `mmap01`, `mmap02`, `mmap03`, `mmap04`, `mmap05`
- `munmap01`, `munmap02`, `munmap03`

### 4. 管道 / 进程等待基础路径

- `pipe01` 到 `pipe15`
- `pipe2_01`, `pipe2_02`, `pipe2_04`
- `wait01`, `wait02`
- `wait401`, `wait402`, `wait403`
- 一部分 `waitpid*` 基础用例

## 二、在现有 syscall 基础上轻度修改即可执行的测试

这类测试已经有 syscall 骨架，主要缺的是细节语义。

### 1. `statx` / `fstatat` / `chmod/chown` 语义补全

- `statx04` 到 `statx12`
- `fstatat*`, `fchmodat*`, `fchownat*`
- `chmod01`, `chmod03`, `chmod05`, `chmod06`, `chmod07`
- `chown01` 到 `chown05`
- `fchown01` 到 `fchown05`
- `lchown01` 到 `lchown03`

原因：

- 当前 `statx/fstat/newfstatat` 已接好
- 但 attribute mask、timestamp、xflag、btime、特殊属性位仍不完整
- `fchmodat/fchownat` 已有实现，但权限、uid/gid 语义仍偏简化

### 2. `openat/fcntl/ioctl/lseek` 角落语义

- `fcntl01` 到 `fcntl39`、`fcntl*_64`
- `flock01` 到 `flock06`
- `ioctl01` 到 `ioctl09`
- `openat2` 之前的多数 `open/openat/creat` errno 类细节用例
- `ftruncate*_64`, `truncate*_64`

原因：

- 入口已存在
- 主要缺命令子集、锁语义、flag 行为、errno 细节、兼容性字段

### 3. `mmap` / `munmap` / `mprotect` / `msync` / `mremap` 的现有 VM 语义细化

- `mmap06` 到 `mmap20`
- `mmapstress01` 到 `mmapstress10`
- `mprotect01` 到 `mprotect05`
- `msync01` 到 `msync04`
- `mremap01` 到 `mremap06`
- `madvise01` 到 `madvise11`
- `mincore01` 到 `mincore04`

原因：

- 当前已经有 `mmap/munmap`
- 但 `MAP_SHARED` 仍按 `MAP_PRIVATE` 处理
- `mprotect` 还是占位返回 0
- `mremap/mincore/msync/madvise` 依赖现有 VMA 与页缓存语义补齐

### 4. `clone/fork/execve/wait` 的边界补齐

- `fork01`, `fork03`, `fork04`, `fork05`, `fork07`, `fork08`, `fork09`, `fork10`, `fork13`, `fork14`
- `clone01` 到 `clone09`
- `execve01` 到 `execve06`
- `execv01`, `execvp01`, `execl01`, `execle01`, `execlp01`
- 一部分 `waitpid*`

原因：

- 当前 `clone/execve/wait4` 已有基础能力
- 但 clone flags、返回值语义、子线程/子进程差异、rusage、errno 还不完整

### 5. 当前占位 syscall 改成真实实现即可受益的测试

- `faccessat01`, `faccessat02`, `faccessat201`, `faccessat202`
- `set_tid_address01`
- `sched_getaffinity01`
- `utimensat01`

原因：

- 当前这些 syscall 不是缺入口，而是入口还在简化返回值 / 固定兼容值

## 三、需要新增 syscall，但实现相对简单的测试

这类测试不需要引入大机制，主要是现有 VFS / task / memory 能力的直接封装。

- `pread01`, `pread02`, `preadv01` 到 `preadv203`
- `pwrite01` 到 `pwrite04`
- `pwritev01` 到 `pwritev202`
- `copy_file_range01` 到 `copy_file_range03`
- `sendfile02` 到 `sendfile09`
- `fsync01` 到 `fsync04`
- `fdatasync01` 到 `fdatasync03`
- `sync01`, `syncfs01`, `sync_file_range01`, `sync_file_range02`
- `close_range01`, `close_range02`
- `getcpu01`
- `readahead01`, `readahead02`
- `fstatfs*`, `statfs*`, `statvfs01`, `statvfs02`
- `memfd_create01` 到 `memfd_create04`
- `fallocate01` 到 `fallocate06`
- `utime01` 到 `utime07`, `utimes01`, `futimesat01`

原因：

- 大多可以直接复用已有 `VFS::read/write/size/truncate/sync` 和时间接口
- 复杂度主要在参数检查、offset 语义、结构体回填

## 四、需要新增 syscall，且相对复杂的测试

这类测试虽然还没到“整套机制”级别，但需要新增较重的等待/事件/文件对象语义。

### 1. `futex` 家族

- `futex_wait01` 到 `futex_wait05`
- `futex_wake01` 到 `futex_wake04`
- `futex_cmp_requeue01`, `futex_cmp_requeue02`
- `futex_wait_bitset01`
- `futex_waitv01` 到 `futex_waitv03`

### 2. `epoll/poll/select/eventfd/timerfd/signalfd`

- `epoll_create*`, `epoll_ctl*`, `epoll_wait*`, `epoll_pwait*`
- `poll01`, `poll02`
- `select01` 到 `select04`
- `ppoll01`
- `pselect01` 到 `pselect03`
- `eventfd01` 到 `eventfd06`
- `eventfd2_01` 到 `eventfd2_03`
- `timerfd*`
- `signalfd01`, `signalfd4_01`, `signalfd4_02`

### 3. 文件事件 / 零拷贝 / 进程句柄

- `inotify01` 到 `inotify12`
- `splice01` 到 `splice09`
- `tee01`, `tee02`
- `vmsplice01` 到 `vmsplice04`
- `pidfd_open*`, `pidfd_send_signal*`, `pidfd_getfd*`

### 4. 进程属性 / 调度属性 / 进程间内存访问

- `prctl01` 到 `prctl10`
- `personality01`, `personality02`
- `sched_getattr*`, `sched_setattr01`
- `process_vm01`, `process_vm_readv02`, `process_vm_readv03`, `process_vm_writev02`

原因：

- 需要把“fd readiness / wait queue / 特殊 fd 对象 / 进程属性对象”做成内核内一致机制
- 不只是补一个 switch case

## 五、需要大幅度修改 Linux subsystem 现有 syscall 语义的测试

这类测试看起来 syscall 名字已经有了，或者已有占位返回值，但想过 LTP 必须重做现有行为模型。

### 1. 信号模型

- `rt_sigaction*`
- `rt_sigprocmask*`
- `sigaction*`, `signal*`, `sigprocmask*`
- `sigsuspend*`, `rt_sigsuspend*`
- `sigtimedwait*`, `sigwait*`, `sigwaitinfo*`
- `sigaltstack*`, `sigpending*`
- `pause01` 到 `pause03`
- `alarm02` 到 `alarm07`
- `setitimer01`, `setitimer02`, `getitimer01`, `getitimer02`
- `kill*`, `tgkill*`, `tkill*` 中依赖真实异步信号投递与处理的部分

原因：

- 当前 Linux subsystem 还没有可靠的 Linux signal disposition / mask / handler / restart / delivery 模型

### 2. 凭据、权限与进程组/会话语义

- `getuid*`, `geteuid*`, `getgid*`, `getegid*`
- `setuid*`, `seteuid*`, `setgid*`, `setegid*`
- `getresuid*`, `setresuid*`, `getresgid*`, `setresgid*`
- `setfsuid*`, `setfsgid*`
- `setgroups*`, `getgroups*`
- `setpgid*`, `getpgid*`, `setsid*`, `getsid*`, `setpgrp*`, `getpgrp*`
- `getpriority*`, `setpriority*`
- `setrlimit*`, `getrlimit*`, `prlimit64`

原因：

- 当前这些大多还是兼容性占位
- 一旦改真，就会反向影响 VFS 权限检查、task 关系、wait、signal、tty、session 语义

### 3. 线程级 clone / TLS / robust list / tid 地址

- `clone3*`
- pthread 风格 `clone*`
- `set_robust_list01`, `get_robust_list01`
- `set_tid_address01`
- `set_thread_area01`

原因：

- 这已经不是“能 fork 一个子任务”就够，而是要把线程组、TLS、退出清理、robust futex list 一起做对

### 4. 现有内存 syscall 的保护与一致性模型

- `mprotect*`
- `msync*`
- `mincore*`
- `madvise*`
- `remap_file_pages*`

原因：

- 这些测试对 VMA、页权限、页脏状态、回写、文件映射一致性要求高
- 当前内存模型需要系统性补齐，不只是补单个入口

## 六、会引入一整套机制的测试

这类测试不建议当前阶段直接碰。它们不属于“补 syscall”，而是“补一个 subsystem”。

### 1. 整套 socket / 网络协议栈

- `socket*`, `bind*`, `listen01`, `accept*`, `connect*`
- `send*`, `recv*`, `recvmsg*`, `sendmsg*`, `sendmmsg*`, `recvmmsg*`
- `getsockopt*`, `setsockopt*`, `getsockname*`, `getpeername*`
- `socketpair*`
- `sockioctl01`
- `network/` 下几乎所有测试

### 2. namespace / cgroup / mount propagation / 新 mount API

- `setns*`, `unshare*`, `pidns*`, `userns*`, `netns*`
- `mount*`, `umount*`, `pivot_root01`
- `fsopen*`, `fsmount*`, `fspick*`, `open_tree*`, `move_mount*`, `mount_setattr01`
- `controllers/` 下全部

### 3. SysV IPC / POSIX MQ

- `shm*`
- `sem*`
- `msg*`
- `mq_*`

### 4. 文件通知 / 安全 / keyring / tracing / 异步 IO 大机制

- `fanotify*`
- `inotify*` 若做完整语义时也会演化成机制层
- `add_key*`, `request_key*`, `keyctl*`
- `bpf*`, `perf_event_open*`
- `io_uring*`
- `io_setup*`, `io_submit*`, `io_getevents*`, `io_destroy*`
- `ptrace*`
- `userfaultfd01`

### 5. 高级内存与平台机制

- `hugetlb/` 下全部
- `thp*`, `ksm*`
- `mbind*`, `get_mempolicy*`, `set_mempolicy*`
- `migrate_pages*`, `move_pages*`
- `memcg*`, `oom*`

### 6. 安全标签 / xattr / ACL / 特定文件系统特性

- `setxattr*`, `getxattr*`, `listxattr*`, `removexattr*`
- `acl1`
- `smack*`
- `ima_*`
- `statx04` 到 `statx10` 中依赖 fs-verity / encrypted / dioalign / immutable / append / nodump 的部分

## 七、实际推进建议

如果目标是 **尽快扩大可运行 LTP 覆盖面**，建议按下面顺序推进：

1. 直接把“当前就可以执行”的测试加入 `module/contest-runner/ltp.h`
2. 先补“轻度修改即可执行”的：
   - `faccessat`
   - `statx` 字段与 flags
   - `mprotect/msync/mremap/mincore` 的最小语义
   - `fcntl` 常用命令
3. 再补“新增但相对简单”的：
   - `pread/pwrite`
   - `fsync/fdatasync/syncfs`
   - `copy_file_range`
   - `close_range`
   - `memfd_create`
4. 然后才考虑：
   - `futex`
   - `epoll/poll/select/eventfd`
   - `signalfd/timerfd`
5. 最后再决定是否进入：
   - signal 完整语义
   - credentials / pgid / session
   - socket / namespace / IPC / mount 新机制

## 八、当前最值得马上加进 runner 的 LTP 用例

如果只想先扩一小批，我建议优先：

- `uname01`
- `getrandom01`
- `sched_yield01`
- `brk01`
- `gettimeofday01`
- `clock_gettime01`
- `getpid01`
- `getcwd01`
- `chdir01`
- `getdents01`
- `openat01`
- `read01`
- `write01`
- `dup01`
- `lseek01`
- `fstat02`
- `statx01`
- `mkdir02`
- `unlink05`
- `rename01`
- `pipe01`
- `mmap01`
- `munmap01`
- `wait01`

这批最容易形成第一轮稳定的 LTP smoke set。
