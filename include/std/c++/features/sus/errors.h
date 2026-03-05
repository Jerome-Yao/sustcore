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

#ifdef __SUS_NO_EXCEPTIONS__

/*
sustcore no exception std
#define SUSNE_STD

#define SUS_EXCEPTION_DEPRECATED                                                                  \
    [[deprecated(                                                                                 \
        "此函数不应被调用!其使用了异常机制, "                                     \
        "但内核中不应使用该机制!请调用其对应的无异常版本(一般命名为 " \
        "sus_xxx)!")]]

#define SUS_EXCEPTION_DEPRECATED
*/

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

#endif