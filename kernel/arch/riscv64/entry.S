/*
 * Sustcore
 * 
 * 作者: theflysong
 * 
 * entry.S
 * 
 * 在此处键入描述
 * 
 */

.section .text.entry, "ax"
.globl _start
_start:
    # 设置栈指针
    la sp, boot_stack_top
    # 跳转到 Rust/C 主函数
    call c_setup

.section .bss.stack
.align 12
boot_stack:
    .space 4096 * 4  # 16KB 栈空间
.globl boot_stack_top
boot_stack_top:
