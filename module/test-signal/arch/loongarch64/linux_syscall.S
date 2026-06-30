#include <syscall.h.in>

    .text
    .global linux_syscall
    .type linux_syscall, @function

linux_syscall:
    move $a7, $a6
    syscall 0
    ret

    .global user_rt_sigreturn_trampoline
    .type user_rt_sigreturn_trampoline, @function

user_rt_sigreturn_trampoline:
    li.d $a7, __NR_rt_sigreturn
    syscall 0
1:
    b 1b
