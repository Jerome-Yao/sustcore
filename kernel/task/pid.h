/**
 * @file pid.h
 * @author your_name (your_email@sth.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/bits.h>

typedef umb_t pid_t;

static pid_t PIDALLOC = 0;

static inline pid_t get_current_pid() {
    return PIDALLOC++;
}
