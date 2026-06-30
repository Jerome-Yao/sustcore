#include <syscall.h.in>

    .text
    .global linux_write
    .type linux_write, @function

linux_write:
    li.d $a7, __NR_write
    syscall 0
    ret
