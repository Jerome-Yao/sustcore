/**
 * @file syscall.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/bits.h>
#include <sus/ctx.h>

/**
 * @brief 通过寄存器上下文获取系统调用参数
 * 
 */
typedef umb_t(*ArgumentGetter)(RegCtx *ctx, int idx);

/**
 * @brief 处理系统调用
 * 
 * @param sysno 系统调用号
 * @param ctx 寄存器上下文
 * @param arg_getter 获取系统调用参数的函数
 * @return umb_t 系统调用返回值
 */
umb_t syscall_handler(int sysno, RegCtx *ctx, ArgumentGetter arg_getter);