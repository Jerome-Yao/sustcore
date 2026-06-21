/**
 * @file prog.h
 * @author OpenAI
 * @brief linux subsystem 中用户程序相关运行时数据声明
 * @version alpha-1.0.0
 * @date 2026-06-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

#include <sustcore/bootstrap.h>
#include <sustcore/capability.h>

extern size_t __prog_heap_base;
extern size_t __prog_brk;
extern CapIdx __prog_pcb_cap;
extern CapIdx __prog_main_tcb_cap;
extern CapIdx __prog_heap_mem_cap;

void init_prog_data(size_t bsargc, const bsheader *bsargv[]);
size_t linux_sys_brk(size_t newbrk);
size_t linux_sys_uname(void *buf);
