#include <sustcore/syscall.h>

    .text

    .global sys_write_serial
    .type sys_write_serial, @function
sys_write_serial:
    move $a2, $a1
    move $a1, $a0
    move $a0, $zero
    li.d $a7, SYS_WRITE_SERIAL
    syscall 0
    ret

    .global sys_mem_resize
    .type sys_mem_resize, @function
sys_mem_resize:
    li.d $a7, SYS_MEM_RESIZE
    syscall 0
    ret

    .global sys_mem_create
    .type sys_mem_create, @function
sys_mem_create:
    move $a4, $a3
    move $a3, $a2
    move $a2, $a1
    move $a1, $a0
    move $a0, $zero
    li.d $a7, SYS_MEM_CREATE
    syscall 0
    ret

    .global sys_pcb_map
    .type sys_pcb_map, @function
sys_pcb_map:
    li.d $a7, SYS_PCB_MAP
    syscall 0
    ret

    .global sys_pcb_unmap
    .type sys_pcb_unmap, @function
sys_pcb_unmap:
    li.d $a7, SYS_PCB_UNMAP
    syscall 0
    ret

    .global sys_pcb_query_vspace
    .type sys_pcb_query_vspace, @function
sys_pcb_query_vspace:
    li.d $a7, SYS_PCB_QUERY_VSPACE
    syscall 0
    ret
