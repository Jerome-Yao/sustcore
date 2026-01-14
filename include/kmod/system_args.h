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
 * @return CapIdx 设备能力
 */
CapIdx sa_get_device(void);

/**
 * @brief 获得PCB能力
 * 
 * @return CapIdx 主线程能力
 */
CapIdx get_pcb_cap(void);

/**
 * @brief 获得主线程能力
 * 
 * @return CapIdx 主线程能力
 */
CapIdx get_main_thread_cap(void);

/**
 * @brief 获得通知能力
 * 
 * @return CapIdx 通知能力
 */
CapIdx get_notification_cap(void);