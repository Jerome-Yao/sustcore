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
    st.d $t6, $sp, 0
    st.d $t3, $sp, 8
    st.d $t4, $sp, 16
    st.d $t5, $sp, 24
    bl linuxss_entry
    ld.d $t6, $sp, 0
    ld.d $a0, $sp, 8
    ld.d $a1, $sp, 16
    ld.d $a2, $sp, 24
    addi.d $sp, $sp, 32
    jr $t6

.Ldispatch_path:
    addi.d $sp, $sp, -(26 * 8)
    st.d $ra, $sp, 0
    st.d $tp, $sp, 8
    st.d $t0, $sp, 16
    st.d $t1, $sp, 24
    st.d $t2, $sp, 32
    st.d $t3, $sp, 40
    st.d $t4, $sp, 48
    st.d $t5, $sp, 56
    st.d $a1, $sp, 64
    st.d $a2, $sp, 72
    st.d $a3, $sp, 80
    st.d $a4, $sp, 88
    st.d $a5, $sp, 96
    st.d $a6, $sp, 104
    st.d $a7, $sp, 112
    st.d $fp, $sp, 120
    st.d $s0, $sp, 128
    st.d $s1, $sp, 136
    st.d $s2, $sp, 144
    st.d $s3, $sp, 152
    st.d $s4, $sp, 160
    st.d $s5, $sp, 168
    st.d $s6, $sp, 176
    st.d $s7, $sp, 184
    st.d $s8, $sp, 192
    st.d $t6, $sp, 200

    move $t3, $sp
    addi.d $sp, $sp, -16
    st.d $t3, $sp, 0
    st.d $t6, $sp, 8
    bl linux_dispatch
    ld.d $t3, $sp, 0
    ld.d $t6, $sp, 8
    move $sp, $t3

.Ldispatch_resume:

    ld.d $ra, $sp, 0
    ld.d $tp, $sp, 8
    ld.d $t0, $sp, 16
    ld.d $t1, $sp, 24
    ld.d $t2, $sp, 32
    ld.d $t3, $sp, 40
    ld.d $t4, $sp, 48
    ld.d $t5, $sp, 56
    ld.d $a1, $sp, 64
    ld.d $a2, $sp, 72
    ld.d $a3, $sp, 80
    ld.d $a4, $sp, 88
    ld.d $a5, $sp, 96
    ld.d $a6, $sp, 104
    ld.d $a7, $sp, 112
    ld.d $fp, $sp, 120
    ld.d $s0, $sp, 128
    ld.d $s1, $sp, 136
    ld.d $s2, $sp, 144
    ld.d $s3, $sp, 152
    ld.d $s4, $sp, 160
    ld.d $s5, $sp, 168
    ld.d $s6, $sp, 176
    ld.d $s7, $sp, 184
    ld.d $s8, $sp, 192
    ld.d $t6, $sp, 200
    addi.d $sp, $sp, (26 * 8)
    jr $t6
