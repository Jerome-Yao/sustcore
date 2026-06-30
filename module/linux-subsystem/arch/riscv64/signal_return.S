    .text
    .global linux_signal_rt_sigreturn_trampoline
    .type linux_signal_rt_sigreturn_trampoline, @function

linux_signal_rt_sigreturn_trampoline:
    li a7, 139
    ecall
1:
    j 1b
