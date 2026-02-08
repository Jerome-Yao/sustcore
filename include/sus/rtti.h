/**
 * @file rtti.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Runtime Type Infomation
 * @version alpha-1.0.0
 * @date 2026-02-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>
#include <type_traits>

// 这个基类是用于实现RTTI的, 其不包含任何成员变量, 仅包含一些虚函数,
// 以供派生类重写 通过这些虚函数, 派生类可以提供自己的类型信息,
// 以供运行时进行类型检查和类型转换

template <typename T, typename BaseType, typename TypeIdEnum>
concept DrvdClassTrait =
    std::is_enum_v<TypeIdEnum> && std::is_base_of_v<BaseType, T> &&
        requires(BaseType *base)
{
    {
        base->type_id()
    } -> std::convertible_to<TypeIdEnum>;
    {
        T::IDENTIFIER
    } -> std::convertible_to<TypeIdEnum>;
};

template <typename BaseType, typename TypeIdEnum>
    requires std::is_enum_v<TypeIdEnum>
class RTTIBase {
protected:
    virtual TypeIdEnum type_id() const = 0;
public:
    template <typename T>
        requires DrvdClassTrait<T, BaseType, TypeIdEnum>
    bool is(void) const {
        return type_id() == T::IDENTIFIER;
    }

    template <typename T>
        requires DrvdClassTrait<T, BaseType, TypeIdEnum>
    static T *cast(BaseType *base) {
        if (base->template is<T>()) {
            return static_cast<T *>(base);
        }
        return nullptr;
    }

    template <typename T>
        requires DrvdClassTrait<T, BaseType, TypeIdEnum>
    static const T *cast(const BaseType *base) {
        if (base->template is<T>()) {
            return static_cast<T *>(base);
        }
        return nullptr;
    }

    template <typename T>
        requires DrvdClassTrait<T, BaseType, TypeIdEnum>
    T *as() {
        if (is<T>()) {
            return static_cast<T *>(this);
        }
        return nullptr;
    }

    template <typename T>
        requires DrvdClassTrait<T, BaseType, TypeIdEnum>
    const T *as() const {
        if (is<T>()) {
            return static_cast<T *>(this);
        }
        return nullptr;
    }
};