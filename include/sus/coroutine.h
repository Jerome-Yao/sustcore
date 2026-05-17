/**
 * @file coroutine.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 协程辅助函数/类定义
 * @version alpha-1.0.0
 * @date 2026-05-17
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <coroutine>
#include <utility>

namespace util {
    template <typename T>
    class cotask;

    /**
     * @brief 协程的 promise 类型定义
     * 
     * @tparam T 协程返回值类型
     */
    template <typename T>
    struct promise {
        using handle_type = std::coroutine_handle<promise<T>>;

        T value{};
        bool detached = false;
        std::coroutine_handle<> continuation = nullptr;

        cotask<T> get_return_object();

        std::suspend_never initial_suspend() noexcept {
            return {};
        }

        struct final_awaiter {
            bool await_ready() const noexcept {
                return false;
            }

            std::coroutine_handle<> await_suspend(
                handle_type handle) const noexcept {
                auto &promise = handle.promise();
                if (promise.detached) {
                    handle.destroy();
                    return std::noop_coroutine();
                }
                if (promise.continuation) {
                    return promise.continuation;
                }
                return std::noop_coroutine();
            }

            void await_resume() const noexcept {}
        };

        final_awaiter final_suspend() noexcept {
            return {};
        }

        void return_value(T ret) {
            value = std::move(ret);
        }

        void unhandled_exception() {
            value = {};
        }
    };

    /**
     * @brief 协程类型定义
     * 
     * @tparam T 协程返回值类型
     */
    template <typename T>
    class cotask {
    public:
        using promise_type = promise<T>;
        using handle_type = typename promise_type::handle_type;

        explicit cotask(handle_type handle) : _handle(handle) {}

        cotask(const cotask &) = delete;
        cotask &operator=(const cotask &) = delete;

        cotask(cotask &&other) noexcept : _handle(other._handle) {
            other._handle = nullptr;
        }

        cotask &operator=(cotask &&other) noexcept {
            if (this != &other) {
                reset();
                _handle = other._handle;
                other._handle = nullptr;
            }
            return *this;
        }

        ~cotask() {
            reset();
        }

        [[nodiscard]]
        bool valid() const noexcept {
            return _handle != nullptr;
        }

        [[nodiscard]]
        bool done() const noexcept {
            return _handle != nullptr && _handle.done();
        }

        T result() const {
            if (_handle == nullptr) {
                return {};
            }
            return _handle.promise().value;
        }

        void resume() const {
            if (_handle != nullptr && !_handle.done()) {
                _handle.resume();
            }
        }

        void detach() {
            if (_handle == nullptr) {
                return;
            }
            _handle.promise().detached = true;
            _handle = nullptr;
        }

        struct awaiter {
            handle_type handle = nullptr;

            bool await_ready() const noexcept {
                return handle == nullptr || handle.done();
            }

            bool await_suspend(std::coroutine_handle<> continuation) const
                noexcept {
                if (handle == nullptr || handle.done()) {
                    return false;
                }
                handle.promise().continuation = continuation;
                return true;
            }

            T await_resume() const {
                if (handle == nullptr) {
                    return {};
                }
                return handle.promise().value;
            }
        };

        awaiter operator co_await() const noexcept {
            return awaiter{_handle};
        }

    private:
        void reset() {
            if (_handle != nullptr) {
                _handle.destroy();
                _handle = nullptr;
            }
        }

        handle_type _handle = nullptr;
    };

    template <typename T>
    cotask<T> promise<T>::get_return_object() {
        return cotask<T>(handle_type::from_promise(*this));
    }
}  // namespace util
