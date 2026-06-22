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
    la.local $t1, g_linux_initialized
    ld.bu $t2, $t1, 0
    bnez $t2, .Ldispatch_path

.Lbootstrap_path:
    move $t3, $a0
    move $t4, $a1
    move $t5, $a2
    move $a0, $sp
    move $a1, $t3
    move $a2, $t4
    move $a3, $t5
    addi.d $sp, $sp, -32
    st.d $t0, $sp, 0
    st.d $t3, $sp, 8
    st.d $t4, $sp, 16
    st.d $t5, $sp, 24
    bl linuxss_entry
    ld.d $t0, $sp, 0
    ld.d $a0, $sp, 8
    ld.d $a1, $sp, 16
    ld.d $a2, $sp, 24
    addi.d $sp, $sp, 32
    jr $t0

.Ldispatch_path:
    addi.d $sp, $sp, -(13 * 8)
    st.d $ra, $sp, 0
    st.d $tp, $sp, 8
    st.d $t0, $sp, 16
    st.d $fp, $sp, 24
    st.d $s0, $sp, 32
    st.d $s1, $sp, 40
    st.d $s2, $sp, 48
    st.d $s3, $sp, 56
    st.d $s4, $sp, 64
    st.d $s5, $sp, 72
    st.d $s6, $sp, 80
    st.d $s7, $sp, 88
    st.d $s8, $sp, 96

    move $t3, $sp
    addi.d $sp, $sp, -16
    st.d $t3, $sp, 0
    bl linux_dispatch
    addi.d $sp, $sp, 16

.Ldispatch_resume:

    ld.d $ra, $sp, 0
    ld.d $tp, $sp, 8
    ld.d $t0, $sp, 16
    ld.d $fp, $sp, 24
    ld.d $s0, $sp, 32
    ld.d $s1, $sp, 40
    ld.d $s2, $sp, 48
    ld.d $s3, $sp, 56
    ld.d $s4, $sp, 64
    ld.d $s5, $sp, 72
    ld.d $s6, $sp, 80
    ld.d $s7, $sp, 88
    ld.d $s8, $sp, 96
    addi.d $sp, $sp, (13 * 8)
    jr $t0
