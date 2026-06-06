/**
 * @file buffer.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 块缓存实现
 * @version alpha-1.0.0
 * @date 2026-06-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <bio/buffer.h>
#include <sustcore/errcode.h>

#include <cstring>

namespace blk {
    BufferCache::BufferCache(IBlockDeviceOps *dev_ops, size_t devno)
        : _dev_ops(dev_ops), _devno(devno), _blksz(0) {
        assert(_dev_ops != nullptr);
        auto block_sz_res = _dev_ops->block_sz();
        assert(block_sz_res.has_value());
        assert(block_sz_res.value() != 0);
        _blksz = block_sz_res.value();
    }

    BufferCache::~BufferCache() {
        auto sync_res = sync_all();
        (void)sync_res;
        for (size_t i = 0; i < MAX_CACHE_SIZE; ++i) {
            if (_buffers[i].get() == nullptr) {
                continue;
            }
            delete[] _buffers[i]->data;
            delete _buffers[i].get();
            _buffers[i] = util::owner<Buffer *>(nullptr);
        }
        _mapping.clear();
    }

    Result<void> BufferCache::writeback_buffer(Buffer &buffer) {
        if (!buffer.valid || !buffer.dirty) {
            void_return();
        }
        auto write_res = _dev_ops->write_blocks(buffer.blkno, buffer.data, 1)
                             .filter(pred::equal_to(1), ErrCode::IO_ERROR)
                             .discard_value();
        propagate(write_res);
        buffer.dirty = false;
        void_return();
    }

    Result<void> BufferCache::clear_slot(size_t idx) {
        if (idx >= MAX_CACHE_SIZE) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        Buffer *buffer = _buffers[idx].get();
        if (buffer == nullptr) {
            void_return();
        }
        if (buffer->refcnt != 0) {
            unexpect_return(ErrCode::BUSY);
        }
        auto erase_it = _mapping.find(buffer->blkno);
        if (erase_it != _mapping.end() && erase_it->second == idx) {
            _mapping.erase(erase_it);
        }
        delete[] buffer->data;
        delete buffer;
        _buffers[idx] = util::owner<Buffer *>(nullptr);
        void_return();
    }

    Result<size_t> BufferCache::find_free() {
        for (size_t i = 0; i < MAX_CACHE_SIZE; ++i) {
            if (_buffers[i].get() == nullptr) {
                return i;
            }
        }

        for (size_t i = 0; i < MAX_CACHE_SIZE; ++i) {
            Buffer *buffer = _buffers[i].get();
            if (buffer == nullptr || buffer->refcnt != 0) {
                continue;
            }
            if (buffer->dirty) {
                auto writeback_res = writeback_buffer(*buffer);
                propagate(writeback_res);
            }
            auto clear_res = clear_slot(i);
            propagate(clear_res);
            return i;
        }

        unexpect_return(ErrCode::BUSY);
    }

    Result<size_t> BufferCache::find_buffer(lba_t blkno) {
        auto map_res = _mapping.at_nt(blkno);
        if (!map_res.has_value()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return map_res.value().get();
    }

    Result<size_t> BufferCache::ensure_buffer(lba_t blkno) {
        auto found_res = find_buffer(blkno);
        if (found_res.has_value()) {
            return found_res.value();
        }
        if (found_res.error() != ErrCode::ENTRY_NOT_FOUND) {
            propagate_return(found_res);
        }

        auto free_res = find_free();
        propagate(free_res);

        auto *buffer = new Buffer{
            .blkno  = blkno,
            .data   = new char[_blksz],
            .dirty  = false,
            .valid  = false,
            .refcnt = 0,
        };
        if (buffer == nullptr || buffer->data == nullptr) {
            delete[] (buffer == nullptr ? nullptr : buffer->data);
            delete buffer;
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        size_t idx    = free_res.value();
        _buffers[idx] = util::owner<Buffer *>(buffer);
        _mapping.insert_or_assign(blkno, idx);
        return idx;
    }

    Result<void> BufferCache::sync(BufferHandler &handler) {
        if (handler._cache != this || handler._buf == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto sync_res = writeback_buffer(*handler._buf);
        propagate(sync_res);
        auto dev_sync_res = _dev_ops->sync();
        propagate(dev_sync_res);
        void_return();
    }

    Result<void> BufferCache::sync_all() {
        for (size_t i = 0; i < MAX_CACHE_SIZE; ++i) {
            Buffer *buffer = _buffers[i].get();
            if (buffer == nullptr) {
                continue;
            }
            auto sync_res = writeback_buffer(*buffer);
            propagate(sync_res);
        }
        auto dev_sync_res = _dev_ops->sync();
        propagate(dev_sync_res);
        void_return();
    }

    Result<void> BufferCache::tidy() {
        auto sync_res = sync_all();
        propagate(sync_res);

        for (size_t i = 0; i < MAX_CACHE_SIZE; ++i) {
            Buffer *buffer = _buffers[i].get();
            if (buffer == nullptr || buffer->refcnt != 0) {
                continue;
            }
            auto clear_res = clear_slot(i);
            propagate(clear_res);
        }
        void_return();
    }

    Result<BufferHandler> BufferCache::get_buffer(lba_t blkno) {
        auto idx_res = ensure_buffer(blkno);
        propagate(idx_res);

        Buffer *buffer = _buffers[idx_res.value()].get();
        if (buffer == nullptr) {
            unexpect_return(ErrCode::UNKNOWN_ERROR);
        }

        if (!buffer->valid) {
            auto read_res = _dev_ops->read_blocks(blkno, buffer->data, 1);
            propagate(read_res);
            if (read_res.value() != 1) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            buffer->valid = true;
        }

        return BufferHandler(util::nnullforce(buffer), *this);
    }
}  // namespace blk
