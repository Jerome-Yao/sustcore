/**
 * @file wait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief thread waiting reasons
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/coroutine.h>
#include <sus/list.h>
#include <sus/map.h>
#include <sustcore/errcode.h>
#include <task/task_struct.h>

#include <concepts>
#include <coroutine>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace task::wait {
    class FutureAwaiter;

    template <typename T>
    class cotask;

    struct promise_base;

    enum class FutureState {
        COMPLETE,
        PENDING,
        ERROR,
    };

    struct WaitContext {
        FutureState state                      = FutureState::COMPLETE;
        WaitReasonId wait_reason               = 0;
        WaitPredicate wait_predicate           = {};
        WaitReadyPredicate ready_predicate     = {};
        std::coroutine_handle<> suspended_leaf = nullptr;

        [[nodiscard]]
        bool pending() const noexcept {
            return state == FutureState::PENDING && wait_reason != 0;
        }

        void clear() noexcept {
            state           = FutureState::COMPLETE;
            wait_reason     = 0;
            wait_predicate  = {};
            ready_predicate = {};
            suspended_leaf  = nullptr;
        }

        static const WaitContext EMPTY;
    };

    struct promise_base {
        bool detached                        = false;
        bool destroy_on_final_suspend        = false;
        std::coroutine_handle<> continuation = nullptr;
        promise_base *wait_parent            = nullptr;
        WaitContext wait_context_storage     = {};

        [[nodiscard]]
        WaitContext &wait_context() noexcept {
            return wait_context_storage;
        }

        [[nodiscard]]
        const WaitContext &wait_context() const noexcept {
            return wait_context_storage;
        }

        void propagate_wait_context_chain() noexcept {
            auto *cursor = wait_parent;
            while (cursor != nullptr) {
                cursor->wait_context_storage = wait_context_storage;
                cursor                       = cursor->wait_parent;
            }
        }

        void clear_wait_context_chain() noexcept {
            wait_context_storage.clear();
            auto *cursor = wait_parent;
            while (cursor != nullptr) {
                cursor->wait_context_storage.clear();
                cursor = cursor->wait_parent;
            }
        }

        auto await_transform(FutureAwaiter awaiter) noexcept;

        template <typename T>
        auto await_transform(cotask<T> &task) noexcept {
            return task.operator co_await();
        }

        template <typename T>
        auto await_transform(const cotask<T> &task) noexcept {
            return task.operator co_await();
        }

        template <typename T>
        auto await_transform(cotask<T> &&task) noexcept {
            return std::move(task).operator co_await();
        }

        template <typename Awaitable>
            requires(
                !std::same_as<std::remove_cvref_t<Awaitable>, FutureAwaiter>)
        auto await_transform(Awaitable &&awaitable);
    };

    namespace detail {
        template <typename Promise>
        concept HasWaitContextAccessor = requires(Promise &promise) {
            {
                promise.wait_context()
            } -> std::same_as<WaitContext &>;
        };

        template <typename T>
        struct is_wait_cotask : std::false_type {};

        template <typename T>
        struct is_wait_cotask<task::wait::cotask<T>> : std::true_type {};

        template <typename Awaitable>
        inline constexpr bool is_wait_cotask_v =
            is_wait_cotask<std::remove_cvref_t<Awaitable>>::value;

        template <typename Awaitable>
        decltype(auto) get_awaiter(Awaitable &&awaitable) {
            if constexpr (requires {
                              std::forward<Awaitable>(awaitable)
                                  .operator co_await();
                          })
            {
                return std::forward<Awaitable>(awaitable).operator co_await();
            } else {
                return std::forward<Awaitable>(awaitable);
            }
        }

        template <typename Awaiter>
        class ClearWaitContextAwaiter {
        private:
            Awaiter _awaiter;
            promise_base *_promise = nullptr;

        public:
            explicit ClearWaitContextAwaiter(Awaiter awaiter,
                                             promise_base *promise) noexcept
                : _awaiter(std::move(awaiter)), _promise(promise) {}

            [[nodiscard]]
            bool await_ready() {
                return _awaiter.await_ready();
            }

            template <typename Promise>
            decltype(auto) await_suspend(
                std::coroutine_handle<Promise> continuation) {
                if (_promise != nullptr) {
                    _promise->clear_wait_context_chain();
                }
                using SuspendRet =
                    decltype(_awaiter.await_suspend(continuation));
                if constexpr (std::is_void_v<SuspendRet>) {
                    _awaiter.await_suspend(continuation);
                } else {
                    return _awaiter.await_suspend(continuation);
                }
            }

            decltype(auto) await_resume() {
                return _awaiter.await_resume();
            }
        };
    }  // namespace detail

    template <typename T>
    struct promise : public promise_base {
        using handle_type = std::coroutine_handle<promise<T>>;

        T value{};

        cotask<T> get_return_object();

        std::suspend_never initial_suspend() noexcept {
            return {};
        }

        struct final_awaiter {
            [[nodiscard]]
            bool await_ready() const noexcept {
                return false;
            }

            std::coroutine_handle<> await_suspend(
                handle_type handle) const noexcept {
                auto &promise = handle.promise();
                promise.clear_wait_context_chain();
                if (promise.continuation) {
                    return promise.continuation;
                }
                if (promise.destroy_on_final_suspend) {
                    handle.destroy();
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

    template <>
    struct promise<void> : public promise_base {
        using handle_type = std::coroutine_handle<promise<void>>;

        cotask<void> get_return_object();

        std::suspend_never initial_suspend() noexcept {
            return {};
        }

        struct final_awaiter {
            [[nodiscard]]
            bool await_ready() const noexcept {
                return false;
            }

            std::coroutine_handle<> await_suspend(
                handle_type handle) const noexcept {
                auto &promise = handle.promise();
                promise.clear_wait_context_chain();
                if (promise.continuation) {
                    return promise.continuation;
                }
                if (promise.destroy_on_final_suspend) {
                    handle.destroy();
                }
                return std::noop_coroutine();
            }

            void await_resume() const noexcept {}
        };

        final_awaiter final_suspend() noexcept {
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {}
    };

    template <typename T>
    class cotask {
    public:
        using promise_type = promise<T>;
        using handle_type  = std::coroutine_handle<promise_type>;

        explicit cotask(handle_type handle) noexcept
            : _handle(handle), _owns_handle(handle != nullptr) {}

        cotask(const cotask &)            = delete;
        cotask &operator=(const cotask &) = delete;

        cotask(cotask &&other) noexcept
            : _handle(other._handle), _owns_handle(other._owns_handle) {
            other._handle      = nullptr;
            other._owns_handle = false;
        }

        cotask &operator=(cotask &&other) noexcept {
            if (this != &other) {
                reset();
                _handle            = other._handle;
                _owns_handle       = other._owns_handle;
                other._handle      = nullptr;
                other._owns_handle = false;
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
            return _handle == nullptr || _handle.done();
        }

        [[nodiscard]]
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

        [[nodiscard]]
        std::coroutine_handle<> handle() const noexcept {
            return _handle;
        }

        [[nodiscard]]
        const WaitContext &wait_context() const noexcept {
            if (_handle == nullptr) {
                return WaitContext::EMPTY;
            }
            return _handle.promise().wait_context();
        }

        void detach() {
            if (_handle == nullptr) {
                return;
            }
            _handle.promise().detached                 = true;
            _handle.promise().destroy_on_final_suspend = true;
            if (_handle.done()) {
                _handle.destroy();
                _handle = nullptr;
            }
            _owns_handle = false;
        }

        struct awaiter {
            cotask *task = nullptr;

            [[nodiscard]]
            bool await_ready() const noexcept {
                return task == nullptr || task->_handle == nullptr ||
                       task->_handle.done();
            }

            template <typename ParentPromise>
            bool await_suspend(std::coroutine_handle<ParentPromise>
                                   continuation) const noexcept {
                if (task == nullptr || task->_handle == nullptr ||
                    task->_handle.done())
                {
                    return false;
                }

                auto &child_promise        = task->_handle.promise();
                child_promise.continuation = continuation;
                if constexpr (detail::HasWaitContextAccessor<ParentPromise>) {
                    child_promise.wait_parent =
                        static_cast<promise_base *>(&continuation.promise());
                    if (child_promise.wait_context().pending()) {
                        child_promise.propagate_wait_context_chain();
                    } else {
                        child_promise.clear_wait_context_chain();
                    }
                } else {
                    child_promise.wait_parent = nullptr;
                    child_promise.wait_context().clear();
                }
                return true;
            }

            T await_resume() const {
                if (task == nullptr || task->_handle == nullptr) {
                    return {};
                }
                auto handle = task->_handle;
                auto value  = handle.promise().value;
                if (handle.promise().detached && !task->_owns_handle) {
                    handle.destroy();
                    task->_handle = nullptr;
                }
                return value;
            }
        };

        awaiter operator co_await() noexcept {
            return awaiter{this};
        }

        awaiter operator co_await() const noexcept {
            return awaiter{const_cast<cotask *>(this)};
        }

    private:
        void reset() {
            if (_handle != nullptr && _owns_handle) {
                _handle.destroy();
            }
            _handle      = nullptr;
            _owns_handle = false;
        }

        handle_type _handle = nullptr;
        bool _owns_handle   = false;
    };

    template <>
    class cotask<void> {
    public:
        using promise_type = promise<void>;
        using handle_type  = std::coroutine_handle<promise_type>;

        explicit cotask(handle_type handle) noexcept
            : _handle(handle), _owns_handle(handle != nullptr) {}

        cotask(const cotask &)            = delete;
        cotask &operator=(const cotask &) = delete;

        cotask(cotask &&other) noexcept
            : _handle(other._handle), _owns_handle(other._owns_handle) {
            other._handle      = nullptr;
            other._owns_handle = false;
        }

        cotask &operator=(cotask &&other) noexcept {
            if (this != &other) {
                reset();
                _handle            = other._handle;
                _owns_handle       = other._owns_handle;
                other._handle      = nullptr;
                other._owns_handle = false;
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
            return _handle == nullptr || _handle.done();
        }

        void resume() const {
            if (_handle != nullptr && !_handle.done()) {
                _handle.resume();
            }
        }

        [[nodiscard]]
        std::coroutine_handle<> handle() const noexcept {
            return _handle;
        }

        [[nodiscard]]
        const WaitContext &wait_context() const noexcept {
            if (_handle == nullptr) {
                return WaitContext::EMPTY;
            }
            return _handle.promise().wait_context();
        }

        void detach() {
            if (_handle == nullptr) {
                return;
            }
            _handle.promise().detached                 = true;
            _handle.promise().destroy_on_final_suspend = true;
            if (_handle.done()) {
                _handle.destroy();
                _handle = nullptr;
            }
            _owns_handle = false;
        }

        struct awaiter {
            cotask *task = nullptr;

            [[nodiscard]]
            bool await_ready() const noexcept {
                return task == nullptr || task->_handle == nullptr ||
                       task->_handle.done();
            }

            template <typename ParentPromise>
            bool await_suspend(std::coroutine_handle<ParentPromise>
                                   continuation) const noexcept {
                if (task == nullptr || task->_handle == nullptr ||
                    task->_handle.done())
                {
                    return false;
                }

                auto &child_promise        = task->_handle.promise();
                child_promise.continuation = continuation;
                if constexpr (detail::HasWaitContextAccessor<ParentPromise>) {
                    child_promise.wait_parent =
                        static_cast<promise_base *>(&continuation.promise());
                    if (child_promise.wait_context().pending()) {
                        child_promise.propagate_wait_context_chain();
                    } else {
                        child_promise.clear_wait_context_chain();
                    }
                } else {
                    child_promise.wait_parent = nullptr;
                    child_promise.wait_context().clear();
                }
                return true;
            }

            void await_resume() const noexcept {
                if (task == nullptr || task->_handle == nullptr) {
                    return;
                }
                auto handle = task->_handle;
                if (handle.promise().detached && !task->_owns_handle) {
                    handle.destroy();
                    task->_handle = nullptr;
                }
            }
        };

        awaiter operator co_await() noexcept {
            return awaiter{this};
        }

        awaiter operator co_await() const noexcept {
            return awaiter{const_cast<cotask *>(this)};
        }

    private:
        void reset() {
            if (_handle != nullptr && _owns_handle) {
                _handle.destroy();
            }
            _handle      = nullptr;
            _owns_handle = false;
        }

        handle_type _handle = nullptr;
        bool _owns_handle   = false;
    };

    /**
     * @brief syscall 协程通用等待点.
     *
     * 该 awaiter 只负责在挂起时写入 promise 的等待上下文, 真正的线程等待
     * 由最外层 syscall 路径统一处理.
     */
    class FutureAwaiter {
    private:
        WaitReasonId _reason = 0;
        WaitPredicate _predicate;
        WaitReadyPredicate _ready_predicate;
        Result<void> _result{};

    public:
        explicit FutureAwaiter(WaitReasonId reason,
                               WaitPredicate predicate            = {},
                               WaitReadyPredicate ready_predicate = {}) noexcept
            : _reason(reason),
              _predicate(std::move(predicate)),
              _ready_predicate(std::move(ready_predicate)),
              _result{} {}

        [[nodiscard]]
        bool ready() const noexcept {
            return _ready_predicate && _ready_predicate();
        }

        [[nodiscard]]
        bool await_ready() const noexcept {
            return ready();
        }

        template <typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
            if constexpr (!detail::HasWaitContextAccessor<Promise>) {
                _result = std::unexpected(ErrCode::INVALID_PARAM);
                return false;
            } else {
                if (_ready_predicate && _ready_predicate()) {
                    return false;
                }
                if (_reason == 0) {
                    _result = std::unexpected(ErrCode::INVALID_PARAM);
                    return false;
                }

                auto &promise                = handle.promise();
                auto &wait_context           = promise.wait_context();
                wait_context.state           = FutureState::PENDING;
                wait_context.wait_reason     = _reason;
                wait_context.wait_predicate  = std::move(_predicate);
                wait_context.ready_predicate = std::move(_ready_predicate);
                wait_context.suspended_leaf  = handle;
                promise.propagate_wait_context_chain();
                _result = {};
                return true;
            }
        }

        [[nodiscard]]
        Result<void> await_resume() const noexcept {
            return _result;
        }
    };

    inline auto promise_base::await_transform(FutureAwaiter awaiter) noexcept {
        return awaiter;
    }

    template <typename Awaitable>
        requires(!std::same_as<std::remove_cvref_t<Awaitable>, FutureAwaiter>)
    inline auto promise_base::await_transform(Awaitable &&awaitable) {
        if constexpr (detail::is_wait_cotask_v<std::remove_cvref_t<Awaitable>>)
        {
            return std::forward<Awaitable>(awaitable).operator co_await();
        } else {
            auto awaiter =
                detail::get_awaiter(std::forward<Awaitable>(awaitable));
            return detail::ClearWaitContextAwaiter<decltype(awaiter)>(
                std::move(awaiter), this);
        }
    }

    template <typename T>
    inline cotask<T> promise<T>::get_return_object() {
        return cotask<T>(handle_type::from_promise(*this));
    }

    inline cotask<void> promise<void>::get_return_object() {
        return cotask<void>(handle_type::from_promise(*this));
    }

    // 等待队列
    struct WaitQueue {
        // 等待队列对应的等待id
        WaitReasonId id;
        util::IntrusiveList<TCB, &TCB::wait_head> threads;

        // 从wait id中创建一个等待队列
        explicit WaitQueue(WaitReasonId id) : id(id), threads() {}
    };

    // 等待原因管理器, 负责管理所有的等待队列
    class WaitReasonManager {
    private:
        // 编号分配
        WaitReasonId _next_reason = 1;
        // wait_id -> wait_queue
        std::unordered_map<WaitReasonId, WaitQueue *> _queues;

        // 获取等待队列, 如果不存在则创建一个新的
        Result<WaitQueue *> queue_for_wait(WaitReasonId id);
        // 获取等待队列, 如果不存在则返回错误
        Result<WaitQueue *> queue_if_exists(WaitReasonId id);

    public:
        // 分配一个 wait reason 号
        WaitReasonId alloc_reason();
        // 将当前线程加入等待队列
        Result<void> enqueue(WaitReasonId id, TCB *tcb);
        Result<void> enqueue(WaitReasonId id, TCB *tcb,
                             WaitPredicate predicate);
        Result<TCB *> peek_one(WaitReasonId id);
        // 从等待队列中弹出一个线程
        Result<TCB *> pop_one(WaitReasonId id);
        Result<void> remove(TCB *tcb);
        // 从等待队列中唤醒一个线程, 返回被唤醒线程的数量(0或1)
        Result<size_t> wake_one(WaitReasonId id);
        // 从等待队列中唤醒所有线程, 返回被唤醒线程的数量
        Result<size_t> wake_all(WaitReasonId id);
        // 判断是否有线程在等待队列中
        bool has_waiting(WaitReasonId id);

        // 初始化等待原因管理器的单例实例
        static void init();
        static bool initialized();
        // 获取等待原因管理器的单例实例
        static WaitReasonManager &inst();
    };

    WaitReasonId alloc_reason();
    [[deprecated("use co_await wait_current(...) in syscall coroutine paths")]]
    Result<void> deprecated_wait_current(WaitReasonId id);
    [[deprecated("use co_await wait_current(...) in syscall coroutine paths")]]
    Result<void> deprecated_wait_current(WaitReasonId id,
                                         WaitPredicate predicate);
    /**
     * @brief 将 syscall 线程登记到等待队列中.
     *
     * 调用者需要先确保 `tcb->syscall_info.handle` 已经指向最内层挂起的
     * coroutine handle.
     */
    Result<void> register_syscall_wait(
        TCB *tcb, const WaitContext &wait_context) noexcept;
    /**
     * @brief 在目标线程真正被调度运行前恢复其挂起的 syscall 协程.
     *
     * @param tcb 即将进入运行态的线程.
     * @return true 该线程可以继续参与本轮调度.
     * @return false 该线程恢复后重新进入等待或恢复失败, 需要跳过.
     */
    [[nodiscard]]
    bool resume_deferred_syscall(TCB *tcb) noexcept;
    Result<TCB *> peek_one(WaitReasonId id);
    Result<size_t> wake_one(WaitReasonId id);
    Result<size_t> wake_all(WaitReasonId id);
    bool has_waiting(WaitReasonId id);
}  // namespace task::wait
