/**
 * @file prm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief paramaters
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sustcore/capability.h>

#include <cstddef>

extern size_t __heap_base;
extern size_t __current_brk;
extern CapIdx __pcb_cap;
extern CapIdx __main_tcb_cap;
extern CapIdx __heap_mem_cap;
extern CapIdx __stack_mem_cap;
