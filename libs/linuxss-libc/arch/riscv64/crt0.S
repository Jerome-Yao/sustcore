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
    sd t6, 8(sp)
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
    ld t6, 8(sp)
    ld a0, 16(sp)
    ld a1, 24(sp)
    ld a2, 32(sp)
    addi sp, sp, 40
    jr t6

.Ldispatch_path:
    addi sp, sp, -(29 * 8)
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd tp, 16(sp)
    sd t0, 24(sp)
    sd t1, 32(sp)
    sd t2, 40(sp)
    sd t3, 48(sp)
    sd t4, 56(sp)
    sd t5, 64(sp)
    sd a1, 72(sp)
    sd a2, 80(sp)
    sd a3, 88(sp)
    sd a4, 96(sp)
    sd a5, 104(sp)
    sd a6, 112(sp)
    sd a7, 120(sp)
    sd s0, 128(sp)
    sd s1, 136(sp)
    sd s2, 144(sp)
    sd s3, 152(sp)
    sd s4, 160(sp)
    sd s5, 168(sp)
    sd s6, 176(sp)
    sd s7, 184(sp)
    sd s8, 192(sp)
    sd s9, 200(sp)
    sd s10, 208(sp)
    sd s11, 216(sp)
    sd t6, 224(sp)

    .option push
    .option norelax
3:
    auipc gp, %pcrel_hi(__global_pointer$)
    addi gp, gp, %pcrel_lo(3b)
    .option pop
    mv t3, sp
    addi sp, sp, -16
    sd t3, 0(sp)
    sd t6, 8(sp)
    call linux_dispatch
    ld t3, 0(sp)
    ld t6, 8(sp)
    mv sp, t3

.Ldispatch_resume:
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld tp, 16(sp)
    ld t0, 24(sp)
    ld t1, 32(sp)
    ld t2, 40(sp)
    ld t3, 48(sp)
    ld t4, 56(sp)
    ld t5, 64(sp)
    ld a1, 72(sp)
    ld a2, 80(sp)
    ld a3, 88(sp)
    ld a4, 96(sp)
    ld a5, 104(sp)
    ld a6, 112(sp)
    ld a7, 120(sp)
    ld s0, 128(sp)
    ld s1, 136(sp)
    ld s2, 144(sp)
    ld s3, 152(sp)
    ld s4, 160(sp)
    ld s5, 168(sp)
    ld s6, 176(sp)
    ld s7, 184(sp)
    ld s8, 192(sp)
    ld s9, 200(sp)
    ld s10, 208(sp)
    ld s11, 216(sp)
    ld t6, 224(sp)
    addi sp, sp, (29 * 8)
    jr t6
