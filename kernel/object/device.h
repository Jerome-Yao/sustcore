/**
 * @file endpoint.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief IPC端点对象
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <object/perm.h>
#include <sus/coroutine.h>
#include <sus/list.h>
#include <sustcore/addr.h>
#include <sustcore/capability.h>
#include <sustcore/msg.h>
#include <task/task_struct.h>
#include <task/wait.h>

#include <cstddef>

namespace cap {

    struct Device {
        VirArea iommu_region;
        size_t virq;
    };
}