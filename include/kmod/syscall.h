/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kmod系统调用接口
 * @version alpha-1.0.0
 * @date 2026-05-14
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cstddef>
#include <cstdint>
#include <sustcore/capability.h>

extern CapIdx __pcb_cap;
extern CapIdx __main_tcb_cap;
extern CapIdx __heap_mem_cap;
extern CapIdx __stack_mem_cap;

enum KmodSchedClass : size_t {
    SCHED_CLASS_FCFS = 2,
    SCHED_CLASS_RR   = 3,
};

struct ForkRet {
    CapIdx ret1;
    size_t ret2;
};

extern "C" {
void kwrites(const char *str, size_t len);
void sys_exit();
bool sys_pcb_kill(CapIdx pcb_cap, int exit_code);
bool sys_pcb_map(CapIdx pcb_cap, CapIdx mem_cap, void *vaddr, uint64_t rwx,
                 uint64_t growth);
CapIdx sys_create_process(const char *path, CapIdx *caps, size_t caps_sz,
                             size_t sched_class);
CapIdx sys_create_thread(void (*entry)(), void *stack_addr, size_t stack_size);
ForkRet sys_fork();
bool sys_execve(const char *path, CapIdx *rsvdlst, size_t rsvdsz);
bool execve(const char *path, CapIdx *rsvdlst, size_t rsvdsz);

bool sys_cap_clone(CapIdx src, CapIdx target);
bool sys_cap_downgrade(CapIdx idx, uint64_t new_perm);
bool sys_cap_derive(CapIdx src, CapIdx target, uint64_t new_perm);
bool sys_cap_remove(CapIdx idx);
bool lookup_cap(CapIdx idx, CapInfo *info);
size_t sys_getpid(CapIdx pcb_cap);

bool sys_create_notification(CapIdx target);
bool sys_signal_notification(CapIdx capidx, size_t idx);
bool sys_unsignal_notification(CapIdx capidx, size_t idx);
bool sys_check_notification(CapIdx capidx, size_t idx);
bool sys_wait_notification(CapIdx capidx, size_t idx);

bool sys_create_endpoint(CapIdx target);
void sys_send_msg(CapIdx endpoint, const void *msgbuf, size_t msgsz,
                  const CapIdx *caplist, size_t capsz);
void sys_recv_msg(CapIdx endpoint, void *msgbuf, size_t *msgsz,
                  CapIdx *caplist, size_t *capsz);
bool sys_try_send_msg(CapIdx endpoint, const void *msgbuf, size_t msgsz,
                      const CapIdx *caplist, size_t capsz);
bool sys_try_recv_msg(CapIdx endpoint, void *msgbuf, size_t *msgsz,
                      CapIdx *caplist, size_t *capsz);

bool sys_mem_create(CapIdx idx, size_t memsz, bool shared, bool continuity,
                    uint64_t growth);
bool sys_mem_map(CapIdx idx, void *vaddr, uint64_t rwx, uint64_t growth);
bool sys_mem_unmap(CapIdx idx, void *vaddr);
bool sys_mem_resize(CapIdx idx, size_t newsz);
ForkRet sys_mem_query(CapIdx idx);
}

extern "C" {
int kputs(const char *str);
size_t brk(size_t newbrk);
void *sbrk(ptrdiff_t increment);
}
