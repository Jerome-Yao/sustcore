/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用接口
 * @version alpha-1.0.0
 * @date 2025-12-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#define SYSCALL_BASE (0xFFFF0000)

#define SYS_EXIT  (SYSCALL_BASE + 0x01)
#define SYS_YIELD (SYSCALL_BASE + 0x02)
#define SYS_LOG   (SYSCALL_BASE + 0x03)