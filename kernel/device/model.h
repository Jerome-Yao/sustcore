/**
 * @file model.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备模型 (ACPI/DTB 的抽象表示)
 * @version alpha-1.0.0
 * @date 2026-05-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/cpu.h>
#include <device/int.h>
#include <logger.h>
#include <sus/owner.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>

#include <vector>

namespace device {
    class DeviceModel;
    /**
     * @brief 内存区域
     *
     */
    struct MemRegion {
        /**
         * @brief 内存状态
         *
         */
        enum class MemoryStatus {
            FREE             = 0,
            RESERVED         = 1,
            ACPI_RECLAIMABLE = 2,
            ACPI_NVS         = 3,
            IOMMU            = 4,
            BAD_MEMORY       = 5
        };

        PhyArea area;
        MemoryStatus status;
    };

    class DeviceProvider {
    public:
        virtual ~DeviceProvider() = default;

        /**
         * @brief 向设备模型注册当前提供者暴露的全部设备信息.
         *
         * @param model 目标设备模型.
         */
        virtual void register_device(DeviceModel &model) const = 0;

        [[nodiscard]]
        virtual const char *name() const = 0;
    };

    class DeviceModel {
    public:
        [[nodiscard]]
        std::vector<MemRegion> memory_regions() {
            return _regions;
        }

        [[nodiscard]]
        CpuGroupInfo &cpus() {
            return _cpus;
        }

        [[nodiscard]]
        IrqManager &interrupt() {
            return _interrupt;
        }

        /**
         * @brief 将一批内存区域并入设备模型.
         *
         * @param regs 待加入的内存区域集合.
         */
        void collect_memory_regions(std::vector<MemRegion> *regs) {
            if (regs == nullptr) {
                return;
            }
            _regions.insert(_regions.end(), regs->begin(), regs->end());
            _regions = _normalize_memory_regions(_regions);
        }

        /**
         * @brief 回写当前系统的 clock virq.
         *
         * @param virq 时钟中断对应的全局 virq.
         */
        void set_clock_virq(virq_t virq) {
            _clock_virq = virq;
        }

        /**
         * @brief 获取当前系统的 clock virq.
         *
         * @return virq_t clock virq.
         */
        [[nodiscard]]
        virq_t clock_virq() const {
            return _clock_virq;
        }

        void register_provider(util::owner<DeviceProvider *> provider) {
            _providers.push_back(std::move(provider));
            provider->register_device(*this);
            loggers::DEVICE::INFO("已注册设备提供者: %s", provider->name());
        }

        DeviceModel(const DeviceModel &)            = delete;
        DeviceModel &operator=(const DeviceModel &) = delete;
        DeviceModel(DeviceModel &&other)            = delete;
        DeviceModel &operator=(DeviceModel &&other) = delete;

        ~DeviceModel() {
            cleanup();
        }
        void cleanup() {
            for (auto &provider : _providers) {
                delete provider.get();
            }
            _providers.clear();
        }

        static DeviceModel &inst();
        static void init();
        static bool initialized();

    protected:
        /**
         * @brief 规范化内存区域列表
         *
         * 该函数接受一个可能包含重叠或未排序的内存区域列表, 并返回一个新的列表,
         * 其会依次执行:
         * 1. 将重叠的同状态区域合并为单个连续区域
         * 2. 从 FREE 状态中剔除任何与非 FREE 区域重叠的部分
         * 3. 如果有两个非 FREE 内存区域重叠, 则将重叠部分设置为 BAD_MEMORY 状态
         * 4. 剔除所有空区域 (大小为 0 的区域, 例如上述结果中的 [0x4000, 0x3000)
         * 就是一个空区域)
         * 5. 将区域按起始地址排序
         *
         * @param regions 待规范化的内存区域列表
         * @return std::vector<MemRegion> 规范化后的内存区域列表
         */
        [[nodiscard]]
        std::vector<MemRegion> _normalize_memory_regions(
            const std::vector<MemRegion> &regions) const;

    private:
        static DeviceModel _INSTANCE;
        static bool _initialized;
        DeviceModel() = default;
        std::vector<util::owner<DeviceProvider *>> _providers;
        std::vector<MemRegion> _regions;
        CpuGroupInfo _cpus;
        IrqManager _interrupt;
        virq_t _clock_virq = 0;
    };

    class KernelProvider : public DeviceProvider {
    public:
        void register_device(DeviceModel &model) const override;
        [[nodiscard]]
        const char *name() const override {
            return "kernel";
        }
    };
};  // namespace device
