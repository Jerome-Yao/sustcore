/**
 * @file buffer.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 缓存
 * @version alpha-1.0.0
 * @date 2026-05-18
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>
#include <sustcore/msg.h>

#include <cstddef>
#include <cstring>
#include <vector>

namespace rpc {
    class bytebuffer {
    public:
        enum direction
        {
            READ = 0,
            WRITE = 1
        };
    private:
        byte *_buf;
        size_t _size = 0;
        size_t _capacity;
        direction _dir;

        Result<void> M_resize(int new_capacity) {
            if (_dir != WRITE)
                unexpect_return(ErrCode::NOT_SUPPORTED);
            byte *oldbuf = _buf;
            _capacity    = new_capacity;
            _buf         = new byte[_capacity];
            if (_buf == nullptr) {
                unexpect_return(ErrCode::ALLOCATION_FAILED);
            }
            memcpy(_buf, oldbuf, _capacity);
            delete[] oldbuf;
            void_return();
        }

        Result<void> M_check_size(int inc_sz) {
            if (_dir != WRITE)
                unexpect_return(ErrCode::NOT_SUPPORTED);
            if (_size + inc_sz > _capacity) {
                auto resize_res = M_resize((_size + inc_sz) * 2);
                propagate(resize_res);
            }
            void_return();
        }

    public:
        constexpr bytebuffer(size_t capacity)
            : _buf(new byte[capacity]), _capacity(capacity), _dir(direction::WRITE) {}
        constexpr bytebuffer(byte *buf, size_t size)
            : _buf(buf), _size(size), _capacity(size), _dir(direction::READ) {}
        ~bytebuffer() {
            delete[] _buf;
        }
        byte *finish() {
            byte *ret = _buf;
            _buf      = nullptr;
            _capacity = _size = 0;
            return ret;
        }
        bytebuffer(const bytebuffer &buffer)
            : _buf(new byte[buffer._capacity]),
              _size(buffer._size),
              _capacity(buffer._capacity) {
            memcpy(_buf, buffer._buf, _size);
        }

        bytebuffer &operator=(const bytebuffer &buffer) {
            if (&buffer == this)
                return *this;
            delete[] finish();
            _buf      = new byte[buffer._capacity];
            _size     = buffer._size;
            _capacity = buffer._capacity;
            memcpy(_buf, buffer._buf, _size);
            return *this;
        }

        bytebuffer(bytebuffer &&buffer)
            : _buf(buffer.finish()),
              _size(buffer._size),
              _capacity(buffer._capacity) {}

        bytebuffer &operator=(bytebuffer &&buffer) {
            if (&buffer == this)
                return *this;
            delete[] finish();
            _buf      = buffer.finish();
            _size     = buffer._size;
            _capacity = buffer._capacity;
            return *this;
        }

        [[nodiscard]] Result<void> writes(const byte *b, size_t sz) {
            if (_dir != WRITE)
                unexpect_return(ErrCode::NOT_SUPPORTED);

            auto check_res = M_check_size(sz);
            propagate(check_res);

            for (int i = 0; i < sz; i++) {
                *(_buf + _size + i) = *(b + i);
            }
            _size += sz;
            void_return();
        }

        template <typename _Tp>
        [[nodiscard]] Result<void> _write_direct(const _Tp &t) {
            return writes(reinterpret_cast<const byte *>(&t), sizeof(_Tp));
        }

        template <typename _Tp>
        [[nodiscard]] Result<void> write(const _Tp &t) {
            if (_dir != WRITE)
                unexpect_return(ErrCode::NOT_SUPPORTED);
            return rpc_bytebuffer_write(*this, t);
        }

        [[nodiscard]] size_t size() const {
            return _size;
        }

        [[nodiscard]] size_t capacity() const {
            return _capacity;
        }

        Result<void> reads(byte *b, size_t sz) {
            if (_dir != READ)
                unexpect_return(ErrCode::NOT_SUPPORTED);
            if (_size + sz > _capacity)
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            for (int i = 0; i < sz; i++) {
                *(b + i) = *(_buf + _size + i);
            }
            _size += sz;
            void_return();
        }

        template <typename _Tp>
        [[nodiscard]] Result<void> _read_direct(_Tp &t)
        {
            return reads(reinterpret_cast<byte *>(&t), sizeof(_Tp));
        }

        template <typename _Tp>
        [[nodiscard]] Result<void> read(_Tp &t) {
            if (_dir != READ)
                unexpect_return(ErrCode::NOT_SUPPORTED);
            return rpc_bytebuffer_read(*this, t);
        }

        template <typename _Tp>
        [[nodiscard]] Result<_Tp> _read_direct()
        {
            if (_dir != READ)
                unexpect_return(ErrCode::NOT_SUPPORTED);
            _Tp t;
            return _read_direct(t).transform(always(t));
        }

        template <typename _Tp>
        [[nodiscard]] Result<_Tp> read() {
            if (_dir != READ)
                unexpect_return(ErrCode::NOT_SUPPORTED);
            _Tp t;
            return rpc_bytebuffer_read(*this, &t).transform(always(t));
        }
    };

    inline Result<void> rpc_bytebuffer_write(bytebuffer &buf, const byte &t) {
        return buf._write_direct(t);
    }

    inline Result<void> rpc_bytebuffer_write(bytebuffer &buf, const uint16_t &t) {
        return buf._write_direct(t);
    }

    inline Result<void> rpc_bytebuffer_write(bytebuffer &buf, const uint32_t &t) {
        return buf._write_direct(t);
    }

    inline Result<void> rpc_bytebuffer_write(bytebuffer &buf, const uint64_t &t) {
        return buf._write_direct(t);
    }

    inline Result<void> rpc_bytebuffer_write(bytebuffer &buf, const int &t) {
        return buf._write_direct(t);
    }

    inline Result<void> rpc_bytebuffer_write(bytebuffer &buf, const bool &t) {
        return buf._write_direct(t);
    }

    // other types
    inline Result<void> rpc_bytebuffer_write(bytebuffer &buf, const ErrCode &t) {
        return buf._write_direct(t);
    }
}  // namespace rpc