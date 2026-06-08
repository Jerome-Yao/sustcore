/**
 * @file features.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief std feature switches
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#define __cpp_lib_atomic_float 201711L
#define __cpp_lib_atomic_lock_free_type_aliases 201907L
#define __cpp_lib_atomic_wait 201907L

#ifdef __SUS_NO_EXCEPTIONS__
#undef __SUS_NO_EXCEPTIONS__
#endif
