/**
 * @file errors.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief errors
 * @version alpha-1.0.0
 * @date 2026-03-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

enum class STDErrorCode {
    Success         = 0,
    OutOfMemory     = 1,
    InvalidArgument = 2,
    RuntimeError    = 3,
    UnknownError    = 4,
    LogicError      = 5,
    OutOfRange      = 6,
    BadAlloc        = 7,
};