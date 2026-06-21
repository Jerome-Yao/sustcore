    .extern linuxss_entry
    .extern linux_dispatch
    .extern g_linux_initialized

    .text
    .global _start
    .global linux_subsystem_entry
    .type _start, @function
    .type linux_subsystem_entry, @function
_start:
linux_subsystem_entry:
    .option push
    .option norelax
1:
    auipc t1, %pcrel_hi(g_linux_initialized)
    addi t1, t1, %pcrel_lo(1b)
    .option pop

    lb t2, 0(t1)
    bnez t2, .Ldispatch_path

.Lbootstrap_path:
    mv t3, a0
    mv t4, a1
    mv t5, a2
    mv a0, sp
    mv a1, t3
    mv a2, t4
    mv a3, t5
    addi sp, sp, -40
    sd gp, 0(sp)
    sd t0, 8(sp)
    sd t3, 16(sp)
    sd t4, 24(sp)
    sd t5, 32(sp)
    .option push
    .option norelax
2:
    auipc gp, %pcrel_hi(__global_pointer$)
    addi gp, gp, %pcrel_lo(2b)
    .option pop
    call linuxss_entry
    ld gp, 0(sp)
    ld t0, 8(sp)
    ld a0, 16(sp)
    ld a1, 24(sp)
    ld a2, 32(sp)
    addi sp, sp, 40
    jr t0

.Ldispatch_path:
    addi sp, sp, -(16 * 8)
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd tp, 16(sp)
    sd t0, 24(sp)
    sd s0, 32(sp)
    sd s1, 40(sp)
    sd s2, 48(sp)
    sd s3, 56(sp)
    sd s4, 64(sp)
    sd s5, 72(sp)
    sd s6, 80(sp)
    sd s7, 88(sp)
    sd s8, 96(sp)
    sd s9, 104(sp)
    sd s10, 112(sp)
    sd s11, 120(sp)

    .option push
    .option norelax
3:
    auipc gp, %pcrel_hi(__global_pointer$)
    addi gp, gp, %pcrel_lo(3b)
    .option pop
    call linux_dispatch

.Ldispatch_resume:
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld tp, 16(sp)
    ld t0, 24(sp)
    ld s0, 32(sp)
    ld s1, 40(sp)
    ld s2, 48(sp)
    ld s3, 56(sp)
    ld s4, 64(sp)
    ld s5, 72(sp)
    ld s6, 80(sp)
    ld s7, 88(sp)
    ld s8, 96(sp)
    ld s9, 104(sp)
    ld s10, 112(sp)
    ld s11, 120(sp)
    addi sp, sp, (16 * 8)
    jr t0
