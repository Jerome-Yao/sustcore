/**
 * @file optional
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 可选值
 * @version alpha-1.0.0
 * @date 2026-02-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <new>
#include <type_traits>
#include <utility>

namespace util {
    template <typename _Fp, typename _Tp>
    concept IfPresentFunc = requires(_Fp f, _Tp t) {
        {
            f(t)
        };
    };

    // _Tp应当为一个POD类型
    enum class HasValueType { SUCCESS = 0, FAILURE = 1 };
    template <typename _Tp, typename _Ep = HasValueType,
              _Ep _Success = _Ep::SUCCESS,
              _Ep _Failure = _Ep::FAILURE>
        requires std::is_enum_v<_Ep>
    class Optional {
    private:
        alignas(_Tp) unsigned char D_storage[sizeof(_Tp)];
        _Ep D_err;

    public:
        using value_type = _Tp;
        template <typename _Up, typename _Ep2 = _Ep>
            requires std::is_enum_v<_Ep2>
        using optional_type = Optional<_Up, _Ep2>;

        Optional() noexcept : D_err(_Failure) {}
        Optional(const _Tp &value) noexcept : D_err(_Success) {
            new (D_storage) _Tp(value);
        }
        Optional(_Tp &&value) noexcept : D_err(_Success) {
            new (D_storage) _Tp(std::move(value));
        }
        Optional(_Ep err) noexcept : D_err(err) {}

        _Tp value() noexcept {
            return *reinterpret_cast<_Tp *>(D_storage);
        }

        _Ep error() const noexcept {
            return D_err;
        }

        bool present() const noexcept {
            return D_err == _Success;
        }

        _Tp &or_else(_Tp &_default) {
            if (present()) {
                return value();
            } else {
                return _default;
            }
        }

        _Tp or_else(_Tp _default) {
            if (present()) {
                return value();
            } else {
                return _default;
            }
        }

        template <typename _Fp>
            requires IfPresentFunc<_Fp, _Tp>
        void if_present(_Fp f) {
            if (present()) {
                f(value());
            }
        }

        template <typename _Up, typename _Fp>
            requires IfPresentFunc<_Fp, _Tp>
        optional_type<_Up> map(_Fp f) {
            if constexpr (std::is_same_v<std::invoke_result_t<_Fp, _Tp>, _Up>) {
                if (present()) {
                    return optional_type<_Up>(f(value()));
                }
                return optional_type<_Up>(error());
            } else if constexpr (std::is_same_v<
                                     std::invoke_result_t<_Fp, _Tp>,
                                     optional_type<_Up>>) {
                if (present()) {
                    return f(value());
                }
                return optional_type<_Up>(error());
            }
            else {
                static_assert(
                    std::is_same_v<std::invoke_result_t<_Fp, _Tp>, _Up> ||
                        std::is_same_v<std::invoke_result_t<_Fp, _Tp>,
                                       optional_type<_Up>>,
                    "Return type of map function must be U or Optional<U>");
            }
        }

        template <typename _Ep2, typename _Fp>
            requires IfPresentFunc<_Fp, _Ep>
        optional_type<_Tp, _Ep2> emap(_Fp f) {
            if constexpr (std::is_same_v<std::invoke_result_t<_Fp, _Ep>, _Ep2>) {
                if (!present()) {
                    return optional_type<_Tp, _Ep2>(f(error()));
                }
                return optional_type<_Tp, _Ep2>(value());
            } else if constexpr (std::is_same_v<
                                     std::invoke_result_t<_Fp, _Ep>,
                                     optional_type<_Tp, _Ep2>>) {
                if (!present()) {
                    return f(error());
                }
                return optional_type<_Tp, _Ep2>(value());
            }
            else {
                static_assert(
                    std::is_same_v<std::invoke_result_t<_Fp, _Ep>, _Ep2> ||
                        std::is_same_v<std::invoke_result_t<_Fp, _Ep>,
                                       optional_type<_Ep2>>,
                    "Return type of emap function must be E2 or Optional<_Tp, E2>");
            }
        }

        template<typename _Fp, typename _Up = std::invoke_result_t<_Fp, _Tp>>
            requires IfPresentFunc<_Fp, _Tp>
        optional_type<_Up> and_then(_Fp f) {
            return map<_Up>(f);
        }

        template<typename _Fp, typename _Up = std::invoke_result_t<_Fp, _Tp>>
            requires IfPresentFunc<_Fp, _Tp> && std::is_same_v<_Up, optional_type<typename _Up::value_type>>
        _Up and_then_opt(_Fp f) {
            return map<typename _Up::value_type>(f);
        }
    };
}  // namespace util