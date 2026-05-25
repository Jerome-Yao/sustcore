/**
 * @file raii.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief raii
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/owner.h>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace util {

    // call the deleter automatically to ensure
    // the resource is correctly released when the guard goes out of scope
    template <typename Deleter>
    class Guard {
    private:
        Deleter deleter;
        bool released = false;

    public:
        Guard(Deleter deleter) : deleter(std::move(deleter)) {}
        Guard(const Guard&)            = delete;
        Guard& operator=(const Guard&) = delete;
        Guard(Guard&&)                 = delete;
        Guard& operator=(Guard&&)      = delete;

        ~Guard() {
            if (!released)
                deleter();
        }
        void release() noexcept {
            released = true;
        }
    };

    // deduction guide for Guard
    template <typename T>
    Guard(T) -> Guard<T>;

    template <typename T>
    struct DefaultDelete {
        constexpr DefaultDelete() = default;

        template <typename U>
            requires(std::is_convertible_v<U*, T*>)
        constexpr DefaultDelete(const DefaultDelete<U>&) noexcept {}

        void operator()(T* ptr) const noexcept {
            static_assert(!std::is_void_v<T>, "DefaultDelete<void> is not supported");
            delete ptr;
        }
    };

    template <typename T, typename Deleter = DefaultDelete<T>>
    class UniquePtr {
    public:
        using pointer      = T*;
        using element_type = T;
        using deleter_type = Deleter;

    private:
        pointer ptr;
        deleter_type deleter;

    public:
        static_assert(!std::is_array_v<T>, "UniquePtr does not support arrays");

        constexpr UniquePtr() noexcept
            requires(std::is_constructible_v<deleter_type>)
            : ptr(nullptr), deleter() {}

        constexpr UniquePtr(std::nullptr_t) noexcept
            requires(std::is_constructible_v<deleter_type>)
            : UniquePtr() {}

        constexpr explicit UniquePtr(pointer p) noexcept
            requires(std::is_constructible_v<deleter_type>)
            : ptr(p), deleter() {}

        constexpr UniquePtr(pointer p, const deleter_type& d) noexcept
            : ptr(p), deleter(d) {}

        constexpr UniquePtr(pointer p, deleter_type&& d) noexcept
            : ptr(p), deleter(std::move(d)) {}

        template <typename U>
            requires(std::is_convertible_v<U*, pointer> &&
                     std::is_constructible_v<deleter_type>)
        constexpr explicit UniquePtr(owner<U*>&& p) noexcept
            : ptr(static_cast<pointer>(p.get())), deleter() {
            p = owner<U*>(nullptr);
        }

        template <typename U>
            requires(std::is_convertible_v<U*, pointer>)
        constexpr UniquePtr(owner<U*>&& p, const deleter_type& d) noexcept
            : ptr(static_cast<pointer>(p.get())), deleter(d) {
            p = owner<U*>(nullptr);
        }

        template <typename U>
            requires(std::is_convertible_v<U*, pointer>)
        constexpr UniquePtr(owner<U*>&& p, deleter_type&& d) noexcept
            : ptr(static_cast<pointer>(p.get())), deleter(std::move(d)) {
            p = owner<U*>(nullptr);
        }

        UniquePtr(const UniquePtr&)            = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;

        constexpr UniquePtr(UniquePtr&& other) noexcept
            : ptr(other.release()), deleter(std::move(other.deleter)) {}

        template <typename U, typename E>
            requires(std::is_convertible_v<U*, pointer> &&
                     std::is_constructible_v<deleter_type, E&&>)
        constexpr UniquePtr(UniquePtr<U, E>&& other) noexcept
            : ptr(other.release()), deleter(std::move(other.get_deleter())) {}

        ~UniquePtr() {
            reset();
        }

        constexpr UniquePtr& operator=(UniquePtr&& other) noexcept
            requires requires(deleter_type& lhs, deleter_type&& rhs) {
                lhs = std::move(rhs);
            } {
            if (this != &other) {
                reset(other.release());
                deleter = std::move(other.deleter);
            }
            return *this;
        }

        template <typename U, typename E>
            requires(std::is_convertible_v<U*, pointer> &&
                     std::is_constructible_v<deleter_type, E&&> &&
                     requires(deleter_type& lhs, E&& rhs) {
                         lhs = std::forward<E>(rhs);
                     })
        constexpr UniquePtr& operator=(UniquePtr<U, E>&& other) noexcept {
            reset(other.release());
            deleter = std::move(other.get_deleter());
            return *this;
        }

        constexpr UniquePtr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        [[nodiscard]]
        constexpr pointer get() const noexcept {
            return ptr;
        }

        [[nodiscard]]
        constexpr deleter_type& get_deleter() noexcept {
            return deleter;
        }

        [[nodiscard]]
        constexpr const deleter_type& get_deleter() const noexcept {
            return deleter;
        }

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept {
            return ptr != nullptr;
        }

        [[nodiscard]]
        constexpr T& operator*() const {
            assert(ptr != nullptr);
            return *ptr;
        }

        [[nodiscard]]
        constexpr pointer operator->() const {
            assert(ptr != nullptr);
            return ptr;
        }

        constexpr pointer release() noexcept {
            pointer old = ptr;
            ptr         = nullptr;
            return old;
        }

        constexpr owner<pointer> release_owner() noexcept {
            return owner<pointer>(release());
        }

        constexpr void reset(pointer new_ptr = nullptr) noexcept {
            if (ptr == new_ptr) {
                return;
            }
            pointer old = ptr;
            ptr         = new_ptr;
            if (old != nullptr) {
                deleter(old);
            }
        }

        constexpr void swap(UniquePtr& other) noexcept {
            using std::swap;
            swap(ptr, other.ptr);
            swap(deleter, other.deleter);
        }
    };

    template <typename T, typename Deleter>
    void swap(UniquePtr<T, Deleter>& lhs, UniquePtr<T, Deleter>& rhs) noexcept {
        lhs.swap(rhs);
    }

    template <typename T>
    UniquePtr(T*) -> UniquePtr<T>;

    template <typename T, typename... Args>
        requires(!std::is_array_v<T>)
    [[nodiscard]]
    UniquePtr<T> make_unique(Args&&... args) {
        T* ptr = new T(std::forward<Args>(args)...);
        if (ptr == nullptr) {
            return UniquePtr<T>();
        }
        return UniquePtr<T>(ptr);
    }
}  // namespace util
