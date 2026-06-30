#include <syscall.h.in>

    .text
    .global linux_syscall
    .type linux_syscall, @function

linux_syscall:
    mv a7, a6
    ecall
    ret

    .global user_rt_sigreturn_trampoline
    .type user_rt_sigreturn_trampoline, @function

user_rt_sigreturn_trampoline:
    li a7, __NR_rt_sigreturn
    ecall
1:
    j 1b
