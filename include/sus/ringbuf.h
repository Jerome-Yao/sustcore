/**
 * @file ringbuf.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 环形队列
 * @version alpha-1.1.0
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
#include <nt/errors.h>
#include <stdexcept>
#include <utility>

namespace util {
    template <typename _Tp, size_t D_capacity>
    class StaticRingBuffer {
    public:
        using IndexType = std::size_t;
        using size_type = std::size_t;

    private:
        // 环形队列
        alignas(_Tp) _Tp D_data[D_capacity];
        IndexType D_head;
        IndexType D_tail;

        constexpr IndexType U_next(IndexType idx) {
            return (idx + 1) % D_capacity;
        }

    public:
        constexpr StaticRingBuffer()
            : D_head(0), D_tail(0) {}
        ~StaticRingBuffer() {
        }

        // 拷贝
        StaticRingBuffer(const StaticRingBuffer& other) {
            D_head = other.D_head;
            D_tail = other.D_tail;
            memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
        }
        StaticRingBuffer& operator=(const StaticRingBuffer& other) {
            if (this != &other) {
                D_head = other.D_head;
                D_tail = other.D_tail;
                memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
            }
            return *this;
        }

        // 移动语义
        constexpr StaticRingBuffer(StaticRingBuffer&& other) noexcept
            : D_head(other.D_head),
              D_tail(other.D_tail) {
            memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
            other.D_head = 0;
            other.D_tail = 0;
        }

        constexpr StaticRingBuffer& operator=(StaticRingBuffer&& other) noexcept {
            if (this != &other) {
                memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
                D_head       = other.D_head;
                D_tail       = other.D_tail;
                other.D_head = 0;
                other.D_tail = 0;
            }
            return *this;
        }

        // capacity
        constexpr size_type size() const noexcept {
            return D_tail >= D_head ? D_tail - D_head : D_capacity - (D_head - D_tail);
        }
        constexpr bool empty() const noexcept {
            return D_tail == D_head;
        }
        constexpr bool full() const noexcept {
            return size() == D_capacity - 1;
        }
        constexpr size_type capacity() const noexcept {
            return D_capacity;
        }

    public:
        // emplace
        template <typename... Args>
        std::result_nt<void> emplace(Args&&... args) noexcept {
            if (full()) {
                return {std::unexpect, std::error_type::OVERFLOW_ERROR};
            }

            const IndexType last_idx = D_tail;
            D_tail                   = U_next(D_tail);

            // 原地构造
            D_data[last_idx] = _Tp(std::forward<Args>(args)...);
            return {};
        }

        // pop/push_front/back
        std::result_nt<void> push(const _Tp& value) noexcept {
            if (full()) {
                return {std::unexpect, std::error_type::OVERFLOW_ERROR};
            }

            const IndexType last_idx = D_tail;
            D_tail                   = U_next(D_tail);

            D_data[last_idx] = value;
            return {};
        }

        std::result_nt<void> pop() noexcept {
            if (empty()) {
                return {std::unexpect, std::error_type::UNDERFLOW_ERROR};
            }
            D_head = U_next(D_head);
            return {};
        }

        _Tp& front() noexcept {
            return D_data[D_head];
        }

        const _Tp& front() const noexcept {
            return D_data[D_head];
        }

        // clear
        void clear() noexcept {
            D_head = D_tail = 0;
        }

        bool contains(const _Tp& value) const noexcept {
            for (IndexType i = D_head; i != D_tail; i = U_next(i)) {
                if (D_data[i] == value) {
                    return true;
                }
            }
            return false;
        }
    };

    /**
     * @brief 动态分配内存的环形队列
     *
     * 该实现保留一个空槽区分“满”和“空”，因此最大可存储元素数是 `capacity() - 1`。
     * 内部存储使用 allocator 显式构造和析构元素，适用于非平凡类型。
     */
    template <typename _Tp, typename Allocator = std::allocator<_Tp>>
    class RingBuffer {
    public:
        using value_type      = _Tp;
        using allocator_type  = Allocator;
        using size_type       = std::size_t;
        using IndexType       = std::size_t;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using pointer         = typename std::allocator_traits<allocator_type>::pointer;
        using const_pointer   = typename std::allocator_traits<allocator_type>::const_pointer;

    private:
        pointer D_data       = nullptr;
        size_type D_capacity = 0;
        IndexType D_head     = 0;
        IndexType D_tail     = 0;
        allocator_type D_alloc;

        [[nodiscard]]
        static constexpr size_type U_normalize_capacity(size_type capacity) noexcept {
            return capacity < 2 ? 2 : capacity;
        }

        [[nodiscard]]
        constexpr IndexType U_next(IndexType idx) const noexcept {
            if (D_capacity == 0) {
                return 0;
            }
            return (idx + 1) % D_capacity;
        }

        [[nodiscard]]
        constexpr IndexType U_index_from_head(size_type offset) const noexcept {
            if (D_capacity == 0) {
                return 0;
            }
            IndexType idx = D_head + offset;
            return idx >= D_capacity ? idx - D_capacity : idx;
        }

        [[nodiscard]]
        constexpr pointer U_allocate(size_type count) {
            if (count == 0) {
                return nullptr;
            }
            return std::allocator_traits<allocator_type>::allocate(D_alloc, count);
        }

        [[nodiscard]]
        constexpr std::result_nt<pointer> U_allocate_nt(size_type count) noexcept {
            if (count == 0) {
                return pointer(nullptr);
            }

            pointer data = U_allocate(count);
            if (data == nullptr) {
                return {std::unexpect, std::error_type::NULLPTR};
            }
            return data;
        }

        constexpr void U_deallocate(pointer data, size_type capacity) noexcept {
            if (data != nullptr) {
                std::allocator_traits<allocator_type>::deallocate(D_alloc, data, capacity);
            }
        }

        template <typename... Args>
        constexpr void U_construct(pointer where, Args&&... args) {
            std::allocator_traits<allocator_type>::construct(
                D_alloc, where, std::forward<Args>(args)...);
        }

        constexpr void U_destroy(pointer where) noexcept {
            std::allocator_traits<allocator_type>::destroy(D_alloc, where);
        }

        constexpr void U_destroy_all() noexcept {
            while (!empty()) {
                U_destroy(D_data + D_head);
                D_head = U_next(D_head);
            }
            D_head = 0;
            D_tail = 0;
        }

        constexpr void U_reset_storage() noexcept {
            U_destroy_all();
            U_deallocate(D_data, D_capacity);
            D_data     = nullptr;
            D_capacity = 0;
        }

        constexpr void U_copy_from(const RingBuffer& other) {
            if (other.D_capacity == 0) {
                return;
            }

            D_capacity = other.D_capacity;
            auto allocated = U_allocate_nt(D_capacity);
            if (!allocated) {
                D_capacity = 0;
                return;
            }

            D_data         = allocated.value();
            const auto cnt = other.size();
            for (size_type i = 0; i < cnt; ++i) {
                const auto src_idx = other.U_index_from_head(i);
                U_construct(D_data + i, other.D_data[src_idx]);
            }
            D_head = 0;
            D_tail = cnt;
        }

    public:
        explicit constexpr RingBuffer(size_type capacity = 8)
            : D_capacity(U_normalize_capacity(capacity)) {
            auto allocated = U_allocate_nt(D_capacity);
            if (!allocated) {
                D_capacity = 0;
                return;
            }
            D_data = allocated.value();
        }

        constexpr ~RingBuffer() {
            U_reset_storage();
        }

        RingBuffer(const RingBuffer& other)
            : D_alloc(other.D_alloc) {
            U_copy_from(other);
        }

        RingBuffer& operator=(const RingBuffer& other) {
            if (this == &other) {
                return *this;
            }

            U_reset_storage();
            D_alloc = other.D_alloc;
            U_copy_from(other);
            return *this;
        }

        constexpr RingBuffer(RingBuffer&& other) noexcept
            : D_data(other.D_data),
              D_capacity(other.D_capacity),
              D_head(other.D_head),
              D_tail(other.D_tail),
              D_alloc(std::move(other.D_alloc)) {
            other.D_data     = nullptr;
            other.D_capacity = 0;
            other.D_head     = 0;
            other.D_tail     = 0;
        }

        constexpr RingBuffer& operator=(RingBuffer&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            U_reset_storage();
            D_data           = other.D_data;
            D_capacity       = other.D_capacity;
            D_head           = other.D_head;
            D_tail           = other.D_tail;
            D_alloc          = std::move(other.D_alloc);
            other.D_data     = nullptr;
            other.D_capacity = 0;
            other.D_head     = 0;
            other.D_tail     = 0;
            return *this;
        }

        [[nodiscard]]
        constexpr size_type size() const noexcept {
            if (D_capacity == 0) {
                return 0;
            }
            return D_tail >= D_head ? D_tail - D_head : D_capacity - (D_head - D_tail);
        }

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return D_head == D_tail;
        }

        [[nodiscard]]
        constexpr bool full() const noexcept {
            return D_capacity != 0 && U_next(D_tail) == D_head;
        }

        [[nodiscard]]
        constexpr size_type capacity() const noexcept {
            return D_capacity;
        }

        template <typename... Args>
        std::result_nt<void> emplace(Args&&... args) noexcept {
            if (D_data == nullptr) {
                return {std::unexpect, std::error_type::NULLPTR};
            }
            if (full()) {
                return {std::unexpect, std::error_type::OVERFLOW_ERROR};
            }

            const IndexType insert_idx = D_tail;
            U_construct(D_data + insert_idx, std::forward<Args>(args)...);
            D_tail = U_next(D_tail);
            return {};
        }

        std::result_nt<void> push(const _Tp& value) noexcept {
            return emplace(value);
        }

        std::result_nt<void> push(_Tp&& value) noexcept {
            return emplace(std::move(value));
        }

        std::result_nt<void> pop() noexcept {
            if (empty()) {
                return {std::unexpect, std::error_type::UNDERFLOW_ERROR};
            }

            U_destroy(D_data + D_head);
            D_head = U_next(D_head);
            if (empty()) {
                D_head = 0;
                D_tail = 0;
            }
            return {};
        }

        reference front() noexcept {
            return D_data[D_head];
        }

        const_reference front() const noexcept {
            return D_data[D_head];
        }

        void clear() noexcept {
            U_destroy_all();
        }

        bool contains(const _Tp& value) const noexcept {
            const auto cnt = size();
            for (size_type i = 0; i < cnt; ++i) {
                if (D_data[U_index_from_head(i)] == value) {
                    return true;
                }
            }
            return false;
        }
    };
}  // namespace util
