/**
 * @file bootstrap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kmod 启动缓冲区中使用的简单引导参数结构
 * @version alpha-1.0.0
 * @date 2026-06-05
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/capability.h>

/**
 * @brief 通过启动缓冲区传递单个 endpoint capability.
 */
struct EndpointBootstrap {
    CapIdx endpoint;
};

/**
 * @brief 通过启动缓冲区传递单个 notification capability.
 */
struct NotifBootstrap {
    CapIdx notif;
};
