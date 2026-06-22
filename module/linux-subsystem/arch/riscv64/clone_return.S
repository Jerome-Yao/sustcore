    .text
    .global linux_clone_fork_return_trampoline
    .type linux_clone_fork_return_trampoline, @function

# a0 = newsp
# a1 = dispatch frame sp captured in linux_subsystem_entry:.Ldispatch_path
linux_clone_fork_return_trampoline:
    mv t1, a0
    mv t2, a1

    ld ra,   0(t2)
    ld gp,   8(t2)
    ld tp,  16(t2)
    ld t0,  24(t2)
    ld s0,  32(t2)
    ld s1,  40(t2)
    ld s2,  48(t2)
    ld s3,  56(t2)
    ld s4,  64(t2)
    ld s5,  72(t2)
    ld s6,  80(t2)
    ld s7,  88(t2)
    ld s8,  96(t2)
    ld s9, 104(t2)
    ld s10,112(t2)
    ld s11,120(t2)

    beqz t1, 1f
    andi t1, t1, -16
    mv sp, t1
    j 2f
1:
    addi sp, t2, 16 * 8
2:
    li a0, 0
    jr t0
