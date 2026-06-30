    .text
    .global linux_signal_rt_sigreturn_trampoline
    .type linux_signal_rt_sigreturn_trampoline, @function

linux_signal_rt_sigreturn_trampoline:
    li.d $a7, 139
    syscall 0
1:
    b 1b
