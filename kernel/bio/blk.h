/**
 * @file blk.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 块设备接口
 * @version alpha-1.0.0
 * @date 2026-06-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <new>
#include <cassert>
#include <unordered_map>
#include <bio/block.h>

namespace blk {
    class BlkManager {
    private:
        BlkManager() = default;
        ~BlkManager() {
            for (auto &[id, device] : _devices) {
                delete device.get();
            }
        }
        static BlkManager _INSTANCE;
        static bool _initialized;

    public:
        static BlkManager &inst() {
            assert(_initialized);
            return _INSTANCE;
        }
        static void init() {
            if (_initialized)
                return;
            _initialized = true;
            new (&_INSTANCE) BlkManager();
        }
        static bool initialized() {
            return _initialized;
        }

    private:
        std::unordered_map<size_t, util::owner<IBlockDeviceOps *> > _devices;
        std::unordered_map<size_t, size_t> _device_ids;
        size_t _next_id = 0;

    public:
        [[nodiscard]]
        Result<bool> contains(size_t id) const {
            return _devices.contains(id);
        }

        [[nodiscard]]
        Result<IBlockDeviceOps *> lookup(size_t id) const {
            auto it = _devices.find(id);
            if (it == _devices.end())
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            return it->second.get();
        }

        [[nodiscard]]
        Result<size_t> find_device_id(IBlockDeviceOps *device) const {
            if (device == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            auto key = reinterpret_cast<size_t>(device);
            auto it = _device_ids.find(key);
            if (it == _device_ids.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return it->second;
        }

        Result<size_t> register_device(util::owner<IBlockDeviceOps *> device) {
            if (device.get() == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            auto key = reinterpret_cast<size_t>(device.get());
            if (_device_ids.contains(key)) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
            size_t id = _next_id++;
            _device_ids.insert_or_assign(key, id);
            _devices.emplace(id, device);
            return id;
        }

        Result<void> unregister_device(size_t id) {
            auto it = _devices.find(id);
            if (it == _devices.end())
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            auto *dev = it->second.get();
            _device_ids.erase(reinterpret_cast<size_t>(dev));
            _devices.erase(it);
            delete dev;
            void_return();
        }
    };
}  // namespace blk
