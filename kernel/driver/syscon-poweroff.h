/**
 * @file syscon-poweroff.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief syscon-poweroff 驱动
 * @version alpha-1.0.0
 * @date 2026-06-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <driver/base.h>
#include <driver/factory.h>

namespace driver {
    class SysconPoweroffDriver final : public DriverBase,
                                       public IShutdownDriver {
    public:
        ~SysconPoweroffDriver() noexcept override;

        void shutdown() noexcept override;

    private:
        SysconPoweroffDriver(DevRes res, char *base, sus_u32 offset,
                             sus_u32 value, sus_u32 reg_io_width,
                             sus_u32 reg_shift) noexcept;

        volatile char *_base  = nullptr;
        sus_u32 _offset       = 0;
        sus_u32 _value        = 0;
        sus_u32 _reg_io_width = 0;
        sus_u32 _reg_shift    = 0;
        size_t _region_size   = 0;

        friend class SysconPoweroffFactory;
    };

    class SysconPoweroffFactory final : public IDeviceFactory {
    public:
        [[nodiscard]]
        const DeviceId &device_id() const noexcept override;

        [[nodiscard]]
        Result<DriverBase *> create(const device::DeviceNode &node,
                                    device::DeviceModel &model,
                                    b64 driver_flag) const override;
    };
}  // namespace driver
