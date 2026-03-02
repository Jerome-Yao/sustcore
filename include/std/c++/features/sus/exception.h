/**
 * @file exception.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief exception
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bits/exception.h>
#include <sus/mstring.h>
#include <type_traits>

/**
 * @brief This function will just print the exception
 * and then halt the system. It will never return.
 * @param s the exception to throw
 */
[[noreturn]]
void __sus_cxa_throw(const std::exception &e);

[[noreturn]]
inline void __sus_throw(const std::exception &e) {
    __sus_cxa_throw(e);
}

[[noreturn]]
inline void __sus_throw(std::exception &&e) {
    __sus_cxa_throw(e);
}

template <typename E>
    requires std::is_pod_v<E>
class __sus_nonclass_data_exception : public std::exception {
public:
    E data;
    util::string _msg;
    constexpr __sus_nonclass_data_exception(E e) noexcept : data(e), _msg(util::to_mstring(e)) {}
    [[nodiscard]]
    const char *what() const noexcept override {
        return _msg.c_str();
    }
#ifdef __SUS_NO_RTTI__
    [[nodiscard]]
    virtual const char* type() const {
        return "__sus_nonclass_data_exception";
    }
#endif
};

template <typename E>
    requires std::is_pod_v<E>
[[noreturn]]
inline void __sus_throw(E e) {
    __sus_cxa_throw(__sus_nonclass_data_exception(e));
}

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define _THROW(exception) __sus_throw(exception)
#define _TRY
#define _CATCH(exception) if (0)
// NOLINTEND(cppcoreguidelines-macro-usage)