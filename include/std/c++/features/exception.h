/**
 * @file exception.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief exception feature selector
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bits/exception.h>
#include <cassert>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#ifdef __cpp_lib_no_exceptions__
#define _THROW(exception) panic(#exception)
#define _TRY
#define _CATCH(exception) if (0)
#define EXCEPTION_DEPRECATED
#else
#define _THROW(exception) throw exception
#define _TRY              try
#define _CATCH(exception) catch (exception)
#define EXCEPTION_DEPRECATED
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)
