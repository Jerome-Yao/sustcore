/**
 * @file schdbase.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief scheduler base
 * @version alpha-1.0.0
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/errcode.h>

enum class ThreadState { EMPTY = 0, READY = 1, RUNNING = 2, YIELD = 3 };

constexpr const char *to_string(ThreadState state) {
    switch (state) {
        case ThreadState::EMPTY:   return "EMPTY";
        case ThreadState::READY:   return "READY";
        case ThreadState::RUNNING: return "RUNNING";
        case ThreadState::YIELD:   return "YIELD";
        default:                   return "UNKNOWN";
    }
}

namespace schd {
    template <typename SU, typename Metadata, typename Tags>
    class SchdBase {
    public:
        using SUType       = SU;
        using MetadataType = Metadata;
        using TagsType     = Tags;
    protected:
        constexpr SchdBase()  = default;
        constexpr ~SchdBase() = default;

        constexpr static SUType *upcast(MetadataType *meta) {
            return static_cast<SUType *>(meta);
        }

        constexpr static MetadataType *downcast(SUType *su) {
            return static_cast<MetadataType *>(su);
        }
    public:
        virtual Result<void> insert(SUType *su) = 0;
        virtual Result<void> remove(SUType *su) = 0;
        virtual SUType *current() = 0;
        // 获得下一个应被调度的SU
        virtual SUType *peer()       = 0;
        // 切换到下一个SU
        virtual SUType *pick()       = 0;
        // 判断是否需要切换到下一个SU
        virtual bool should_switch() = 0;

        // yield current SU
        virtual void yield()   = 0;
        // 将当前SU移出调度器
        virtual void suspend() = 0;
    };
}  // namespace schd