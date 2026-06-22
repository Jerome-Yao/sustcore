/**
 * @file syscon-poweroff.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief syscon-poweroff 驱动
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/fdt/device_node.h>
#include <device/model.h>
#include <device/resource.h>
#include <driver/syscon-poweroff.h>
#include <logger.h>

namespace {
    constexpr driver::FDTDeviceId SYSCON_POWEROFF_FDT_IDS[] = {
        {.compatible = "syscon-poweroff", .driver_flag = 0},
        {.compatible = nullptr, .driver_flag = 0},
    };
    constexpr driver::DeviceId SYSCON_POWEROFF_DEVICE_ID = {
        .fdt_ids = SYSCON_POWEROFF_FDT_IDS,
        .pci_ids = nullptr,
    };

    [[nodiscard]]
    Result<sus_u32> load_u32_prop(const fdt::FDTDeviceNode &node,
                                  std::string_view prop_name) noexcept {
        auto prop_res = node.property(prop_name);
        if (!prop_res.has_value()) {
            loggers::DEVICE::ERROR("节点 %s 缺少 %s 属性", node.name(),
                                   prop_name.data());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        const auto &prop = *prop_res;
        if (prop.type() != fdt::DevicePropView::PropType::INTEGER) {
            loggers::DEVICE::ERROR("节点 %s 的 %s 属性类型非法", node.name(),
                                   prop_name.data());
            unexpect_return(ErrCode::INVALID_PROPERTY_TYPE);
        }
        return static_cast<sus_u32>(prop.as_integer(sizeof(sus_u32)));
    }

    [[nodiscard]]
    Result<sus_u32> maybe_load_u32_prop(const fdt::FDTDeviceNode &node,
                                        std::string_view prop_name) noexcept {
        auto prop_res = node.property(prop_name);
        if (!prop_res.has_value()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        const auto &prop = *prop_res;
        if (prop.type() != fdt::DevicePropView::PropType::INTEGER) {
            loggers::DEVICE::ERROR("节点 %s 的 %s 属性类型非法", node.name(),
                                   prop_name.data());
            unexpect_return(ErrCode::INVALID_PROPERTY_TYPE);
        }
        return static_cast<sus_u32>(prop.as_integer(sizeof(sus_u32)));
    }

}  // namespace

namespace driver {
    SysconPoweroffDriver::SysconPoweroffDriver(DevRes res, char *base,
                                               sus_u32 offset, sus_u32 value,
                                               sus_u32 reg_io_width,
                                               sus_u32 reg_shift) noexcept
        : DriverBase(std::move(res)),
          _base(base),
          _offset(offset),
          _value(value),
          _reg_io_width(reg_io_width),
          _reg_shift(reg_shift),
          _region_size(_mmios.empty() || _mmios.front().get() == nullptr
                           ? 0
                           : _mmios.front()->region().size()) {}

    SysconPoweroffDriver::~SysconPoweroffDriver() noexcept {
        auto *platform =
            device::DeviceModel::initialized()
                ? device::DeviceModel::inst().platform()
                : nullptr;
        if (platform != nullptr) {
            platform->clear_shutdown_driver(this);
        }
    }

    void SysconPoweroffDriver::shutdown() noexcept {
        if (_base == nullptr || _mmios.empty() || _mmios.front().get() == nullptr) {
            loggers::DEVICE::FATAL("syscon-poweroff 未持有有效 MMIO 资源");
            panic("syscon-poweroff 未持有有效 MMIO 资源");
        }

        const auto reg_offset =
            static_cast<size_t>(_offset) << static_cast<size_t>(_reg_shift);
        if (reg_offset + _reg_io_width > _region_size) {
            loggers::DEVICE::FATAL(
                "syscon-poweroff 寄存器越界: offset=0x%llx width=%u size=0x%llx",
                static_cast<unsigned long long>(reg_offset), _reg_io_width,
                static_cast<unsigned long long>(_region_size));
            panic("syscon-poweroff 寄存器越界");
        }

        auto *reg = _base + reg_offset;
        loggers::DEVICE::INFO(
            "执行 syscon-poweroff: base=%p offset=0x%llx reg=%p value=0x%x width=%u shift=%u",
            _base, static_cast<unsigned long long>(reg_offset), reg, _value,
            _reg_io_width, _reg_shift);
        switch (_reg_io_width) {
            case 1:
                *reinterpret_cast<volatile sus_u8 *>(reg) =
                    static_cast<sus_u8>(_value);
                break;
            case 2:
                *reinterpret_cast<volatile sus_u16 *>(reg) =
                    static_cast<sus_u16>(_value);
                break;
            case 4:
                *reinterpret_cast<volatile sus_u32 *>(reg) = _value;
                break;
            case 8:
                *reinterpret_cast<volatile sus_u64 *>(reg) = _value;
                break;
            default:
                loggers::DEVICE::FATAL("syscon-poweroff 不支持的访问宽度: %u",
                                       _reg_io_width);
                panic("syscon-poweroff 不支持的访问宽度");
        }

        while (true);
    }

    const DeviceId &SysconPoweroffFactory::device_id() const noexcept {
        return SYSCON_POWEROFF_DEVICE_ID;
    }

    Result<DriverBase *> SysconPoweroffFactory::create(
        const device::DeviceNode &node, device::DeviceModel &model,
        b64 driver_flag) const {
        (void)driver_flag;
        auto *platform = model.platform();
        if (platform == nullptr) {
            loggers::DEVICE::ERROR(
                "syscon-poweroff 创建失败: platform 尚未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        if (platform->shutdown_driver() != nullptr) {
            loggers::DEVICE::ERROR(
                "syscon-poweroff 创建失败: platform 已绑定 shutdown driver");
            unexpect_return(ErrCode::KEY_DUPLICATED);
        }

        auto *fdt_node = node.as<fdt::FDTDeviceNode>();
        if (fdt_node == nullptr) {
            loggers::DEVICE::ERROR(
                "syscon-poweroff 创建失败: 节点 %s 不是 FDT 节点", node.name());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto regmap_res = load_u32_prop(*fdt_node, "regmap");
        propagate(regmap_res);
        auto offset_res = load_u32_prop(*fdt_node, "offset");
        propagate(offset_res);
        auto value_res = load_u32_prop(*fdt_node, "value");
        propagate(value_res);

        sus_u32 reg_io_width = 4;
        sus_u32 reg_shift    = 0;
        fdt::FDTDeviceNode *syscon_fdt_node = nullptr;
        for (const auto &owned_candidate : model.device_nodes()) {
            auto *candidate = owned_candidate.get();
            auto *candidate_fdt =
                candidate != nullptr ? candidate->as<fdt::FDTDeviceNode>()
                                     : nullptr;
            if (candidate_fdt == nullptr) {
                continue;
            }
            if (candidate_fdt->raw_node().phandle == regmap_res.value()) {
                syscon_fdt_node = candidate_fdt;
                break;
            }
        }
        if (syscon_fdt_node == nullptr) {
            loggers::DEVICE::ERROR(
                "syscon-poweroff 创建失败: 找不到 regmap phandle=%u 对应 syscon 节点",
                regmap_res.value());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto reg_io_width_res =
            maybe_load_u32_prop(*syscon_fdt_node, "reg-io-width");
        if (reg_io_width_res.has_value()) {
            reg_io_width = reg_io_width_res.value();
        } else if (reg_io_width_res.error() != ErrCode::ENTRY_NOT_FOUND) {
            propagate_return(reg_io_width_res);
        }
        auto reg_shift_res =
            maybe_load_u32_prop(*syscon_fdt_node, "reg-shift");
        if (reg_shift_res.has_value()) {
            reg_shift = reg_shift_res.value();
        } else if (reg_shift_res.error() != ErrCode::ENTRY_NOT_FOUND) {
            propagate_return(reg_shift_res);
        }

        if (reg_io_width != 1 && reg_io_width != 2 && reg_io_width != 4 &&
            reg_io_width != 8)
        {
            loggers::DEVICE::ERROR(
                "syscon-poweroff 创建失败: 非法 reg-io-width=%u",
                reg_io_width);
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto virqs = device::DevResManager::get_virq_resource(node);
        auto mmios =
            device::DevResManager::get_mmio_resource(*syscon_fdt_node);
        if (mmios.empty()) {
            loggers::DEVICE::ERROR(
                "syscon-poweroff 创建失败: syscon 节点缺少 MMIO 资源");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto *mmio = mmios.front().get();
        if (mmio == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        loggers::DEVICE::INFO(
            "syscon-poweroff 解析 syscon MMIO: node=%s pa=[%p,%p) size=0x%llx width=%u",
            syscon_fdt_node->name(), mmio->region().begin.addr(),
            mmio->region().end.addr(),
            static_cast<unsigned long long>(mmio->region().size()),
            reg_io_width);
        auto map_res = device::MMIOManager::inst().map_to_kernel(*mmio);
        if (!map_res.has_value()) {
            loggers::DEVICE::ERROR(
                "syscon-poweroff 创建失败: MMIO 映射失败 err=%s",
                to_cstring(map_res.error()));
            propagate_return(map_res);
        }

        auto *driver = new SysconPoweroffDriver(
            DriverBase::DevRes(node, std::move(virqs), std::move(mmios)),
            map_res.value().as<char>(), offset_res.value(), value_res.value(),
            reg_io_width, reg_shift);
        if (driver == nullptr) {
            loggers::DEVICE::ERROR("syscon-poweroff 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        platform->set_shutdown_driver(driver);
        loggers::DEVICE::INFO(
            "已注册 syscon-poweroff shutdown 驱动: node=%s regmap=%u offset=0x%x value=0x%x",
            node.name(), regmap_res.value(), offset_res.value(),
            value_res.value());
        return driver;
    }
}  // namespace driver
