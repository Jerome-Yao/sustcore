/**
 * @file move.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief move, forward, and other utilities
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bits/qualifier.h>

namespace std {
    template <typename T>
    constexpr remove_reference_t<T>&& move(T&& t) {
        return static_cast<remove_reference_t<T>&&>(t);
    }

    template <typename T>
    constexpr T&& forward(remove_reference_t<T>& t) {
        return static_cast<T&&>(t);
    }

    template <typename T>
    constexpr T&& forward(remove_reference_t<T>&& t) {
        return static_cast<T&&>(t);
    }
}  // namespace std