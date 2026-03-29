/**
 * @file hook.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief hooks for scheduler to collect information
 * @version alpha-1.0.0
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/units.h>

#include <concepts>

namespace schd {
    namespace tags {
        struct tag {};
        struct empty : public tag {};
        struct on_tick : public tag {};
    }  // namespace tags

    template <typename SchdPolicy>
    class SchdHooks {
    public:
        using SchdPolicyType = SchdPolicy;
        using SchdPolicyTags = typename SchdPolicyType::Tags;

    protected:
        static void on_tick(SchdPolicy &pol, units::tick gap_ticks) {
            if constexpr (std::derived_from<SchdPolicyTags, tags::on_tick>) {
                pol.on_tick(gap_ticks);
            }
        }
    };
};  // namespace schd