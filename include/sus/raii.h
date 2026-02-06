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

namespace util {
    template <typename T>
    class DefaultDeleter {
    public:
        void operator()(T resource) const noexcept {
            delete resource;
        }
    };

    template <typename T>
    class ArrayDeleter {
    public:
        void operator()(T resource) const noexcept {
            delete[] resource;
        }
    };

    template <typename T, typename Deleter = DefaultDeleter<T>>
    class Raii {
    private:
        T *resource;
        Deleter deleter;
    public:
        Raii(T *resource, Deleter deleter = Deleter()) : resource(resource), deleter(deleter) {}
        ~Raii() {
            if (resource != nullptr)
                deleter(resource);
        }
        T *get() const noexcept {
            return resource;
        }
        T *move() const noexcept {
            T *res = resource;
            resource = nullptr;
            return res;
        }

        Raii(const Raii<T, Deleter>&) = delete;
        Raii<T, Deleter>& operator=(const Raii<T, Deleter>&) = delete;

        Raii(Raii<T, Deleter>&& other) noexcept : resource(other.resource), deleter(other.deleter) {
            other.resource = nullptr;
        }

        Raii<T, Deleter>& operator=(Raii<T, Deleter>&& other) noexcept {
            if (this != &other) {
                deleter(resource);
                resource       = other.resource;
                deleter        = other.deleter;
                other.resource = nullptr;
            }
            return *this;
        }
    };

    template <typename T>
    using ARaii = Raii<T, ArrayDeleter<T>>;
}