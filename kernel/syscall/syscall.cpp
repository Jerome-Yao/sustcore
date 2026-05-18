/**
 * @file syscall.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用
 * @version alpha-1.0.0
 * @date 2026-05-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <env.h>
#include <logger.h>
#include <sustcore/addr.h>
#include <sustcore/syscall.h>
#include <syscall/cap.h>
#include <syscall/endpoint.h>
#include <syscall/memory.h>
#include <syscall/notif.h>
#include <syscall/syscall.h>
#include <syscall/task.h>
#include <syscall/uaccess.h>
#include <task/task_struct.h>

#include <cassert>

namespace syscall {
    void write_serial(const UString &str, size_t len) {
        sys_write_serial(str.kbuf(), len);
    }

    constexpr size_t MAX_SYSCALL_PATH = 256;

    const char *name_of(b64 sysno) {
        switch (sysno) {
            case SYS_WRITE_SERIAL:        return "SYS_WRITE_SERIAL";
            case SYS_CREATE_PROCESS:      return "SYS_CREATE_PROCESS";
            case SYS_PCB_KILL:            return "SYS_PCB_KILL";
            case SYS_YIELD:               return "SYS_YIELD";
            case SYS_LOG:                 return "SYS_LOG";
            case SYS_FORK:                return "SYS_FORK";
            case SYS_GETPID:              return "SYS_GETPID";
            case SYS_CREATE_THREAD:       return "SYS_CREATE_THREAD";
            case SYS_YIELD_THREAD:        return "SYS_YIELD_THREAD";
            case SYS_EXECVE:              return "SYS_EXECVE";
            case SYS_PCB_MAP:             return "SYS_PCB_MAP";
            case SYS_NOTIF_CREATE:        return "SYS_NOTIF_CREATE";
            case SYS_NOTIF_SIGNAL:        return "SYS_NOTIF_SIGNAL";
            case SYS_NOTIF_UNSIGNAL:      return "SYS_NOTIF_UNSIGNAL";
            case SYS_NOTIF_CHECK:         return "SYS_NOTIF_CHECK";
            case SYS_NOTIF_WAIT:          return "SYS_NOTIF_WAIT";
            case SYS_CAP_CLONE:           return "SYS_CAP_CLONE";
            case SYS_CAP_DOWNGRADE:       return "SYS_CAP_DOWNGRADE";
            case SYS_CAP_DERIVE:          return "SYS_CAP_DERIVE";
            case SYS_CAP_LOOKUP:          return "SYS_CAP_LOOKUP";
            case SYS_CAP_REMOVE:          return "SYS_CAP_REMOVE";
            case SYS_ENDPOINT_CREATE:     return "SYS_ENDPOINT_CREATE";
            case SYS_ENDPOINT_SEND:       return "SYS_ENDPOINT_SEND";
            case SYS_ENDPOINT_RECV:       return "SYS_ENDPOINT_RECV";
            case SYS_ENDPOINT_SEND_ASYNC: return "SYS_ENDPOINT_SEND_ASYNC";
            case SYS_ENDPOINT_RECV_ASYNC: return "SYS_ENDPOINT_RECV_ASYNC";
            case SYS_ENDPOINT_CALL:       return "SYS_ENDPOINT_CALL";
            case SYS_ENDPOINT_REPLY:      return "SYS_ENDPOINT_REPLY";
            case SYS_MEM_CREATE:          return "SYS_MEM_CREATE";
            case SYS_MEM_UNMAP:           return "SYS_MEM_UNMAP";
            case SYS_MEM_RESIZE:          return "SYS_MEM_RESIZE";
            case SYS_MEM_QUERY:           return "SYS_MEM_QUERY";
            default:                      return "UNKNOWN_SYSCALL";
        }
    }

    static RetPack finish_syscall(const util::nonnull<Riscv64Context *> &ctx,
                                  const util::nonnull<task::TCB *> &tcb,
                                  RetPack ret) {
        ctx->write_ret(ret);
        ctx->sepc                    += 4;
        tcb->coroutines.syscall_done  = true;
        return ret;
    }

    util::cotask<RetPack> entrance(const util::nonnull<Riscv64Context *> &ctx,
                                   const util::nonnull<task::TCB *> &tcb) {
        ArgPack args = ctx->read_args();

        // 参数解包
        b64 arg0 = args.args[0];
        b64 arg1 = args.args[1];
        b64 arg2 = args.args[2];
        b64 arg3 = args.args[3];

        b64 sysno  = args.syscall_number;
        b64 capidx = args.capidx;

        b64 ret0 = 0, ret1 = 0;

        bool processed = true;

        // capidx (a0) is the primary capability slot; args[] carry
        // operation-specific values starting at a1.
        switch (sysno) {
            // Basic process / memory syscalls.
            case SYS_WRITE_SERIAL: {
                write_serial(UString((VirAddr)arg0, arg1), arg1);
                ret0 = ret1 = 0;
                break;
            }
            case SYS_CREATE_PROCESS: {
                ret0 = pcb_create_process(
                    capidx, UString((VirAddr)arg0, MAX_SYSCALL_PATH),
                    VirAddr(arg1), arg2, arg3);
                ret1 = 0;
                break;
            }
            case SYS_CREATE_THREAD: {
                ret0 = pcb_create_thread(capidx, VirAddr(arg0), VirAddr(arg1),
                                         arg2);
                ret1 = 0;
                break;
            }
            case SYS_FORK: {
                auto fork_ret = pcb_fork(capidx);
                ret0          = fork_ret.child_pcb_cap;
                ret1          = fork_ret.child_pid;
                break;
            }
            case SYS_EXECVE: {
                bool current_target = pcb_is_current(capidx);
                bool ok             = pcb_execve(
                    capidx, UString((VirAddr)arg0, MAX_SYSCALL_PATH),
                    VirAddr(arg1), arg2);
                if (ok) {
                    if (current_target) {
                        ret0       = ctx->regs[Context::A0_BASE];
                        ret1       = ctx->regs[Context::A0_BASE + 1];
                        ctx->sepc -= 4;
                    } else {
                        ret0 = true;
                        ret1 = 0;
                    }
                } else {
                    ret0 = false;
                    ret1 = 0;
                }
                break;
            }
            case SYS_PCB_KILL: {
                ret0 = pcb_kill(capidx, static_cast<int>(arg0));
                ret1 = 0;
                break;
            }
            case SYS_GETPID: {
                ret0 = get_pid(capidx);
                ret1 = 0;
                break;
            }

            // Notification object operations.
            case SYS_NOTIF_WAIT: {
                ret0 = wait_notification(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_NOTIF_SIGNAL: {
                ret0 = notification_signal(capidx, arg0, true);
                ret1 = 0;
                break;
            }
            case SYS_NOTIF_UNSIGNAL: {
                ret0 = notification_signal(capidx, arg0, false);
                ret1 = 0;
                break;
            }
            case SYS_NOTIF_CHECK: {
                ret0 = check_notification(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_NOTIF_CREATE: {
                ret0 = notification_create(capidx);
                ret1 = 0;
                break;
            }

            // Generic capability operations.
            case SYS_CAP_CLONE: {
                ret0 = cap_clone(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_CAP_DOWNGRADE: {
                ret0 = cap_downgrade(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_CAP_DERIVE: {
                ret0 = cap_derive(capidx, arg0, arg1);
                ret1 = 0;
                break;
            }
            case SYS_CAP_LOOKUP: {
                ret0 = sys_cap_lookup(capidx, VirAddr(arg0));
                ret1 = 0;
                break;
            }
            case SYS_CAP_REMOVE: {
                ret0 = cap_remove(capidx);
                ret1 = 0;
                break;
            }
            case SYS_ENDPOINT_CREATE: {
                ret0 = endpoint_create(capidx);
                ret1 = 0;
                break;
            }
            case SYS_ENDPOINT_SEND: {
                ret0 = endpoint_send(capidx, VirAddr(arg0), true);
                ret1 = 0;
                break;
            }
            case SYS_ENDPOINT_RECV: {
                auto recv_task = endpoint_recv_sync(capidx, VirAddr(arg0));
                RetPack ret = co_await recv_task;
                co_return finish_syscall(ctx, tcb, ret);
            }
            case SYS_ENDPOINT_SEND_ASYNC: {
                ret0 = endpoint_send(capidx, VirAddr(arg0), false);
                ret1 = 0;
                break;
            }
            case SYS_ENDPOINT_RECV_ASYNC: {
                ret0 = endpoint_recv_async(capidx, VirAddr(arg0));
                ret1 = 0;
                break;
            }
            case SYS_MEM_CREATE: {
                ret0 = mem_create(capidx, arg0, arg1, arg2,
                                  static_cast<cap::MemoryGrowth>(arg3));
                ret1 = 0;
                break;
            }
            case SYS_PCB_MAP: {
                ret0 = pcb_map(capidx, static_cast<CapIdx>(arg0), VirAddr(arg1),
                               static_cast<PageMan::RWX>(arg2),
                               static_cast<cap::MemoryGrowth>(arg3));
                ret1 = 0;
                break;
            }
            case SYS_MEM_UNMAP: {
                ret0 = mem_unmap(capidx, VirAddr(arg0));
                ret1 = 0;
                break;
            }
            case SYS_MEM_RESIZE: {
                ret0 = mem_resize(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_MEM_QUERY: {
                auto query = mem_query(capidx);
                ret0       = query.memsz;
                ret1       = query.allocated;
                break;
            }
            case SYS_ENDPOINT_CALL: {
                auto call_task =
                    endpoint_call(capidx, VirAddr(arg0), VirAddr(arg1));
                RetPack ret = co_await call_task;
                co_return finish_syscall(ctx, tcb, ret);
            }
            case SYS_ENDPOINT_REPLY: {
                ret0 = endpoint_reply(capidx, VirAddr(arg0));
                ret1 = 0;
                break;
            }
            default: {
                processed = false;
                loggers::SYSCALL::ERROR("未知的系统调用号: %d", sysno);
                ret0 = ret1 = 0;
                break;
            }
        }

        co_return finish_syscall(ctx, tcb, RetPack{processed, ret0, ret1});
    };
}  // namespace syscall
