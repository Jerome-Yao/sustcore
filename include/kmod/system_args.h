/**
 * @file system_args.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统参数
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <kmod/capability.h>

/**
 * @brief 获得系统传递的设备能力
 *
 * @return CapPtr 设备能力
 */
CapPtr sa_get_device(void);