/**
 * @file stringfwd.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief string的forward declaration
 * @version alpha-1.0.0
 * @date 2026-03-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cstddef>
#include <memory>

namespace std {
    template <class CharT>
    class char_traits;

    template <class CharT, class traits = char_traits<CharT>>
    class basic_string_view;

    template <class CharT, class Traits = char_traits<CharT>,
              class Allocator = allocator<CharT>>
    class basic_string;

    using string    = basic_string<char>;
    using wstring   = basic_string<wchar_t>;
    using u8string  = basic_string<char8_t>;
    using u16string = basic_string<char16_t>;
    using u32string = basic_string<char32_t>;

    using string_view    = basic_string_view<char>;
    using wstring_view   = basic_string_view<wchar_t>;
    using u8string_view  = basic_string_view<char8_t>;
    using u16string_view = basic_string_view<char16_t>;
    using u32string_view = basic_string_view<char32_t>;
}