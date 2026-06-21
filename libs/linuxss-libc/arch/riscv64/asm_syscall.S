#include <sustcore/syscall.h>

    .text
    .global sys_write_serial
    .type   sys_write_serial, @function
sys_write_serial:
    mv a2, a1
    mv a1, a0
    li a0, 0
    li a7, SYS_WRITE_SERIAL
    ecall
    ret

    .global sys_mem_resize
    .type   sys_mem_resize, @function
sys_mem_resize:
    li a7, SYS_MEM_RESIZE
    ecall
    ret

    .global sys_mem_create
    .type   sys_mem_create, @function
sys_mem_create:
    mv a4, a3
    mv a3, a2
    mv a2, a1
    mv a1, a0
    li a0, 0
    li a7, SYS_MEM_CREATE
    ecall
    ret

    .global sys_pcb_map
    .type   sys_pcb_map, @function
sys_pcb_map:
    li a7, SYS_PCB_MAP
    ecall
    ret

    .global sys_pcb_unmap
    .type   sys_pcb_unmap, @function
sys_pcb_unmap:
    li a7, SYS_PCB_UNMAP
    ecall
    ret

    .global sys_pcb_query_vspace
    .type   sys_pcb_query_vspace, @function
sys_pcb_query_vspace:
    li a7, SYS_PCB_QUERY_VSPACE
    ecall
    ret
