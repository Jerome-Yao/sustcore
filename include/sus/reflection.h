/**
 * @file reflection.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief C++ 静态反射
 * @version alpha-1.0.0
 * @date 2026-05-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <concepts>
#include <string_view>
#include <meta>

/**
 * @brief 自动序列化函数
 *
 * @tparam E 枚举类型
 * @param value 枚举值
 * @return 枚举值名
 */
template<typename E>
    requires std::is_enum_v<E>
constexpr std::string_view enum_to_string(E value) {
    // 'template for' 在编译期展开循环
    template for (constexpr auto e : std::meta::enumerators_of(^^E)) {
        // [:e:] 语法将编译期反射对象 e 还原为对应的运行时值
        constexpr auto name = std::meta::identifier_of(e);
        if (value == [:e:]) {
            return name;
        }
    }
    return "<unnamed>";
}
