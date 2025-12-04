/**
 * @file uaccess.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief User space memory access接口
 * @version alpha-1.0.0
 * @date 2025-12-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <sus/bits.h>
#include <sus/ctx.h>

// U-Access类原语
// 与vmm区别: 要求此时使用用户进程的页表.
// 并且不会自己主动去遍历页表, 而是直接访问虚拟地址,
// 由硬件自动进行地址转换.

/**
 * @brief 开始访问用户空间内存
 * 
 * 由arch实现.
 * 不推荐轻易调用此函数。
 * 
 */
void ua_start_access(void);

/**
 * @brief 结束访问用户空间内存
 * 
 * 由arch实现.
 * 不推荐轻易调用此函数。
 * 
 */
void ua_end_access(void);

/**
 * @brief uaccess 内存复制函数
 * 
 * @param dst 目的
 * @param src 源
 * @param size 大小
 */
void ua_memcpy(void *dst, const void *src, size_t size);

/**
 * @brief uaccess 字符串复制函数
 * 
 * @param dst 目的
 * @param src 源
 */
void ua_strcpy(char *dst, const char *src);

/**
 * @brief uaccess 字符串长度函数
 * 
 * @param str 源
 */
size_t ua_strlen(const char *str);