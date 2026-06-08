/**
 * @file supports.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief std supports
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#define __CXX_HAS_PLATFORM_WAIT 1

#ifdef __STDCPP_FLOAT16_T__
#define __CXX_HAS_FLOAT16_T 1
#else
#define __CXX_HAS_FLOAT16_T 0
#endif

#ifdef __STDCPP_FLOAT32_T__
#define __CXX_HAS_FLOAT32_T 1
#else
#define __CXX_HAS_FLOAT32_T 0
#endif

#ifdef __STDCPP_FLOAT64_T__
#define __CXX_HAS_FLOAT64_T 1
#else
#define __CXX_HAS_FLOAT64_T 0
#endif

#ifdef __STDCPP_FLOAT128_T__
#define __CXX_HAS_FLOAT128_T 1
#else
#define __CXX_HAS_FLOAT128_T 0
#endif

#ifdef __STDCPP_BFLOAT16_T__
#define __CXX_HAS_BFLOAT16_T 1
#else
#define __CXX_HAS_BFLOAT16_T 0
#endif
