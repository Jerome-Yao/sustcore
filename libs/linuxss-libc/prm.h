/**
 * @file prm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief linux subsystem libc runtime parameters
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <elf.h>

#include <sustcore/bootstrap.h>

extern "C" bool g_linux_initialized;
extern "C" size_t linuxss_entry(const void *stack_sp, size_t init_a0,
                                size_t init_a1, size_t init_a2);
extern "C" void linux_main(const void *stack_sp, size_t argc, const char *argv[],
                           const char *envp[], const Elf64_auxv_t *auxv,
                           size_t bsargc, const bsheader *bsargv[]);
extern "C" size_t linux_dispatch(size_t a0, size_t a1, size_t a2, size_t a3,
                                 size_t a4, size_t a5, size_t a6, size_t a7);
