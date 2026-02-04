/**
 * @file pair.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief pair
 * @version alpha-1.0.0
 * @date 2026-02-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

namespace util {
    template <typename _T1, typename _T2>
    struct Pair {
        _T1 first;
        _T2 second;

        constexpr Pair() = default;

        constexpr Pair(const _T1 &a, const _T2 &b) : first(a), second(b) {}

        constexpr Pair(_T1 &&a, _T2 &&b)
            : first(static_cast<_T1 &&>(a)), second(static_cast<_T2 &&>(b)) {}
    };
}