    .text
    .global linux_clone_fork_return_trampoline
    .type linux_clone_fork_return_trampoline, @function

# $a0 = newsp
# $a1 = dispatch frame sp captured in linux_subsystem_entry:.Ldispatch_path
linux_clone_fork_return_trampoline:
    move $t1, $a0
    move $t2, $a1

    ld.d $ra, $t2, 0
    ld.d $tp, $t2, 8
    ld.d $t0, $t2, 16
    ld.d $fp, $t2, 24
    ld.d $s0, $t2, 32
    ld.d $s1, $t2, 40
    ld.d $s2, $t2, 48
    ld.d $s3, $t2, 56
    ld.d $s4, $t2, 64
    ld.d $s5, $t2, 72
    ld.d $s6, $t2, 80
    ld.d $s7, $t2, 88
    ld.d $s8, $t2, 96

    beqz $t1, 1f
    srli.d $t3, $t1, 4
    slli.d $t1, $t3, 4
    move $sp, $t1
    b 2f
1:
    addi.d $sp, $t2, (13 * 8)
2:
    move $a0, $zero
    jr $t0
