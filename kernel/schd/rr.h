/**
 * @file rr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Round Robin 调度器
 * @version alpha-1.0.0
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <schd/hook.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sustcore/errcode.h>

namespace schd::rr {
    class Metadata {
    public:
        ThreadState state                   = ThreadState::EMPTY;
        int slice_cnt                       = 0;
        util::ListHead<Metadata> _schd_head = {};

        Metadata()  = default;
        ~Metadata() = default;
    };

    class tags : public schd::tags::on_tick {};

    template <typename SU>
    class RR : public SchdBase<SU, Metadata, tags> {
    public:
        using SUType       = SU;
        using MetadataType = Metadata;
        using Tags         = tags;
        using Base         = SchdBase<SUType, MetadataType, Tags>;
        using Base::downcast;
        using Base::upcast;
        constexpr static int TIME_SLICES = 5;

    private:
        util::IntrusiveList<MetadataType, &MetadataType::_schd_head>
            _ready_queue    = {};
        SUType *_current_su = nullptr;

        constexpr static bool P_ready(const MetadataType &meta) {
            return meta.state == ThreadState::READY;
        }

        constexpr static bool P_runnable(const MetadataType &meta) {
            return meta.state == ThreadState::READY ||
                   meta.state == ThreadState::RUNNING;
        }

        constexpr static bool P_rescheduable(const MetadataType &meta) {
            return meta.state == ThreadState::READY ||
                   meta.state == ThreadState::YIELD ||
                   meta.state == ThreadState::RUNNING;
        }

    public:
        constexpr RR()  = default;
        constexpr ~RR() = default;

        virtual Result<void> insert(SUType *su) override {
            if (su == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            auto meta       = downcast(su);
            meta->state     = ThreadState::READY;
            meta->slice_cnt = 0;
            _ready_queue.push_back(*meta);
            void_return();
        }

        virtual Result<void> remove(SUType *su) override {
            if (su == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            auto meta = downcast(su);
            if (!_ready_queue.contains(*meta)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            _ready_queue.remove(*meta);
            meta->state = ThreadState::EMPTY;
            void_return();
        }

        virtual SUType *current() override {
            return _current_su;
        }

        virtual SUType *peer() override {
            if (_ready_queue.empty()) {
                return _current_su;
            }
            auto meta = &_ready_queue.front();
            return upcast(meta);
        }

        virtual SUType *pick() override {
            if (_ready_queue.empty()) {
                return _current_su;
            }
            auto next_meta = &_ready_queue.front();
            _ready_queue.pop_front();
            if (_current_su) {
                auto current_meta = downcast(_current_su);
                if (P_rescheduable(*current_meta)) {
                    current_meta->state = ThreadState::READY;
                    _ready_queue.push_back(*current_meta);
                }
            }
            _current_su          = upcast(next_meta);
            next_meta->state     = ThreadState::RUNNING;
            next_meta->slice_cnt = 0;
            return _current_su;
        }

        virtual bool should_switch() override {
            if (_ready_queue.empty()) {
                return false;
            }

            if (_current_su == nullptr) {
                return true;
            }

            auto current_meta = downcast(_current_su);
            // 当前线程不可继续运行, 或者时间片已用尽, 都需要切换
            return !P_runnable(*current_meta) ||
                   current_meta->slice_cnt >= TIME_SLICES;
        }

        virtual void yield() override {
            if (_current_su) {
                auto current_meta   = downcast(_current_su);
                current_meta->state = ThreadState::YIELD;
            }
        }

        virtual void suspend() override {
            if (_current_su) {
                auto current_meta   = downcast(_current_su);
                current_meta->state = ThreadState::EMPTY;
                _current_su         = nullptr;
            }
        }

        virtual void on_tick(units::tick /*gap_ticks*/) {
            if (_current_su) {
                auto current_meta = downcast(_current_su);
                current_meta->slice_cnt += 1;
            }
        }
    };
}  // namespace schd::rr