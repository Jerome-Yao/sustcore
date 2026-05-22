/**
 * @file config.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备配置
 * @version alpha-1.0.0
 * @date 2026-05-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/device/fdt_helper.h>
#include <device/config.h>
#include <logger.h>
#include <libfdt.h>

#include <cstring>

namespace {
    class DeviceTreeParser {
    private:
        static constexpr const char *PROP_COMPATIBLE          = "compatible";
        static constexpr const char *PROP_REG                 = "reg";
        static constexpr const char *PROP_STATUS              = "status";
        static constexpr const char *PROP_INTERRUPTS          = "interrupts";
        static constexpr const char *PROP_INTERRUPTS_EXTENDED = "interrupts-extended";
        static constexpr const char *PROP_INTERRUPT_PARENT    = "interrupt-parent";
        static constexpr const char *PROP_INTERRUPT_CELLS     = "#interrupt-cells";
        static constexpr const char *PROP_INTERRUPT_CONTROLLER =
            "interrupt-controller";
        static constexpr const char *PROP_PHANDLE       = "phandle";
        static constexpr const char *PROP_LINUX_PHANDLE = "linux,phandle";

        static const char *node_name(const FDTNodeDesc& node) {
            const char *name = fdt_get_name(FDTHelper::fdt, node, nullptr);
            return name == nullptr ? "<unknown>" : name;
        }

        static bool has_property(const FDTNodeDesc& node, const char *name) {
            return FDTHelper::get_property(node, name) >= 0;
        }

        static bool status_is_disabled(const FDTNodeDesc& node) {
            int len            = 0;
            const char *status = static_cast<const char *>(
                fdt_getprop(FDTHelper::fdt, node, PROP_STATUS, &len));
            if (status == nullptr || len <= 0) {
                return false;
            }
            return strncmp(status, "disabled", len) == 0 ||
                   strncmp(status, "reserved", len) == 0;
        }

        static bool read_u32_property(const FDTNodeDesc& node,
                                      const char *name, b32& value) {
            FDTPropDesc prop = FDTHelper::get_property(node, name);
            if (prop < 0) {
                return false;
            }
            auto val = FDTHelper::get_property_value(prop);
            if (val.ptr == nullptr || val.len < sizeof(fdt32_t)) {
                loggers::DEVICE::ERROR("节点 %s 的属性 %s 长度非法: %u",
                                       node_name(node), name,
                                       static_cast<unsigned int>(val.len));
                return false;
            }
            value = fdt32_to_cpu(*static_cast<const fdt32_t *>(val.ptr));
            return true;
        }

        static int interrupt_cells_for_phandle(b32 phandle) {
            if (phandle == 0 || phandle == FDT_MAX_PHANDLE) {
                loggers::DEVICE::ERROR("非法中断控制器 phandle: %u", phandle);
                return -1;
            }

            FDTNodeDesc controller =
                fdt_node_offset_by_phandle(FDTHelper::fdt, phandle);
            if (controller < 0) {
                loggers::DEVICE::ERROR("未找到中断控制器 phandle=%u", phandle);
                return -1;
            }

            b32 cells = 0;
            if (!read_u32_property(controller, PROP_INTERRUPT_CELLS, cells)) {
                loggers::DEVICE::ERROR(
                    "中断控制器 %s 缺少 %s 属性",
                    node_name(controller), PROP_INTERRUPT_CELLS);
                return -1;
            }

            return static_cast<int>(cells);
        }

        static bool find_interrupt_parent(const FDTNodeDesc& node,
                                          b32& phandle) {
            FDTNodeDesc current = node;
            while (current >= 0) {
                if (read_u32_property(current, PROP_INTERRUPT_PARENT, phandle)) {
                    return true;
                }
                current = fdt_parent_offset(FDTHelper::fdt, current);
            }
            return false;
        }

        static b32 parse_default_phandle(const FDTNodeDesc& node) {
            b32 phandle = fdt_get_phandle(FDTHelper::fdt, node);
            if (phandle != 0) {
                return phandle;
            }

            b32 linux_phandle = 0;
            if (read_u32_property(node, PROP_LINUX_PHANDLE, linux_phandle)) {
                return linux_phandle;
            }
            return 0;
        }

        static void push_irq_desc(std::vector<RawIrqDesc>& interrupts,
                                  b32 phandle, const fdt32_t *cells,
                                  int cell_count) {
            RawIrqDesc desc;
            desc.phandle = phandle;
            for (int i = 0; i < cell_count; i++) {
                desc.hw_irqs.push_back(fdt32_to_cpu(cells[i]));
            }
            interrupts.push_back(desc);
        }

        static RawCompatibleConfig as_compatible_config(
            const RawDeviceConfig& device) {
            RawCompatibleConfig compatible;
            compatible.node_name   = device.node_name;
            compatible.compatibles = device.compatibles;
            compatible.properties  = device.properties;
            return compatible;
        }

        static RawRegionalConfig as_regional_config(
            const RawDeviceConfig& device) {
            RawRegionalConfig regional;
            regional.node_name   = device.node_name;
            regional.compatibles = device.compatibles;
            regional.properties  = device.properties;
            regional.regions     = device.regions;
            return regional;
        }

    public:
        static bool is_device_node(const FDTNodeDesc& node) {
            if (node <= 0) {
                return false;
            }

            if (status_is_disabled(node)) {
                loggers::DEVICE::DEBUG("跳过禁用设备树节点: %s",
                                       node_name(node));
                return false;
            }

            return has_property(node, PROP_COMPATIBLE) ||
                   has_property(node, PROP_REG) ||
                   has_property(node, PROP_INTERRUPTS) ||
                   has_property(node, PROP_INTERRUPTS_EXTENDED) ||
                   has_property(node, PROP_INTERRUPT_CONTROLLER) ||
                   has_property(node, PROP_PHANDLE) ||
                   has_property(node, PROP_LINUX_PHANDLE);
        }

        static void parse_compatibles(const FDTNodeDesc& node,
                                      std::vector<std::string>& compatibles)
        {
            int count =
                fdt_stringlist_count(FDTHelper::fdt, node, PROP_COMPATIBLE);
            if (count == -FDT_ERR_NOTFOUND) {
                return;
            }
            if (count < 0) {
                loggers::DEVICE::ERROR("节点 %s 的 compatible 属性非法: %s",
                                       node_name(node), fdt_strerror(count));
                return;
            }

            for (int i = 0; i < count; i++) {
                int len         = 0;
                const char *str = fdt_stringlist_get(
                    FDTHelper::fdt, node, PROP_COMPATIBLE, i, &len);
                if (str == nullptr || len <= 0) {
                    loggers::DEVICE::ERROR(
                        "节点 %s 的 compatible[%d] 读取失败",
                        node_name(node), i);
                    continue;
                }
                compatibles.push_back(std::string(str));
                loggers::DEVICE::DEBUG("节点 %s compatible: %s",
                                       node_name(node), str);
            }
        }

        static void parse_regions(const FDTNodeDesc& node,
                                  std::vector<VirArea>& regions)
        {
            FDTPropDesc prop = FDTHelper::get_property(node, PROP_REG);
            if (prop < 0) {
                return;
            }

            auto val       = FDTHelper::get_property_value(prop);
            int addr_cells = FDTHelper::get_parent_addresses_cells(node);
            int size_cells = FDTHelper::get_parent_size_cells(node);
            int cells_per_region = addr_cells + size_cells;

            if (val.ptr == nullptr || cells_per_region <= 0 ||
                val.len % (cells_per_region * sizeof(fdt32_t)) != 0)
            {
                loggers::DEVICE::ERROR(
                    "节点 %s 的 reg 属性长度非法: len=%u addr_cells=%d size_cells=%d",
                    node_name(node), static_cast<unsigned int>(val.len),
                    addr_cells, size_cells);
                return;
            }

            int region_count =
                static_cast<int>(val.len / (cells_per_region * sizeof(fdt32_t)));
            const fdt32_t *cells = static_cast<const fdt32_t *>(val.ptr);

            for (int i = 0; i < region_count; i++) {
                addr_t begin = 0;
                size_t size  = 0;

                int base = i * cells_per_region;
                for (int j = 0; j < addr_cells; j++) {
                    begin = (begin << 32) | fdt32_to_cpu(cells[base + j]);
                }
                for (int j = 0; j < size_cells; j++) {
                    size = (size << 32) |
                           fdt32_to_cpu(cells[base + addr_cells + j]);
                }

                if (size == 0) {
                    loggers::DEVICE::DEBUG("节点 %s 跳过空 reg 区域 %d",
                                           node_name(node), i);
                    continue;
                }

                VirArea area(VirAddr(begin), VirAddr(begin + size));
                regions.push_back(area);
                loggers::DEVICE::DEBUG("节点 %s reg[%d]: [%p, %p)",
                                       node_name(node), i, area.begin.addr(),
                                       area.end.addr());
            }
        }

        static void parse_interrupts(const FDTNodeDesc& node,
                                     std::vector<RawIrqDesc>& interrupts)
        {
            FDTPropDesc ext_prop =
                FDTHelper::get_property(node, PROP_INTERRUPTS_EXTENDED);
            if (ext_prop >= 0) {
                auto val = FDTHelper::get_property_value(ext_prop);
                if (val.ptr == nullptr || val.len % sizeof(fdt32_t) != 0) {
                    loggers::DEVICE::ERROR(
                        "节点 %s 的 interrupts-extended 属性长度非法: %u",
                        node_name(node),
                        static_cast<unsigned int>(val.len));
                    return;
                }

                const fdt32_t *cells = static_cast<const fdt32_t *>(val.ptr);
                int total_cells = static_cast<int>(val.len / sizeof(fdt32_t));
                int idx         = 0;
                while (idx < total_cells) {
                    b32 phandle = fdt32_to_cpu(cells[idx++]);
                    int irq_cells = interrupt_cells_for_phandle(phandle);
                    if (irq_cells < 0 || idx + irq_cells > total_cells) {
                        loggers::DEVICE::ERROR(
                            "节点 %s 的 interrupts-extended 描述不完整",
                            node_name(node));
                        return;
                    }

                    push_irq_desc(interrupts, phandle, &cells[idx], irq_cells);
                    idx += irq_cells;
                }
                return;
            }

            FDTPropDesc prop = FDTHelper::get_property(node, PROP_INTERRUPTS);
            if (prop < 0) {
                return;
            }

            b32 phandle = 0;
            if (!find_interrupt_parent(node, phandle)) {
                loggers::DEVICE::ERROR("节点 %s 缺少 interrupt-parent",
                                       node_name(node));
                return;
            }

            int irq_cells = interrupt_cells_for_phandle(phandle);
            if (irq_cells <= 0) {
                loggers::DEVICE::ERROR("节点 %s 的 #interrupt-cells 非法: %d",
                                       node_name(node), irq_cells);
                return;
            }

            auto val = FDTHelper::get_property_value(prop);
            int bytes_per_irq = irq_cells * static_cast<int>(sizeof(fdt32_t));
            if (val.ptr == nullptr || val.len % bytes_per_irq != 0) {
                loggers::DEVICE::ERROR(
                    "节点 %s 的 interrupts 属性长度非法: len=%u cells=%d",
                    node_name(node), static_cast<unsigned int>(val.len),
                    irq_cells);
                return;
            }

            const fdt32_t *cells = static_cast<const fdt32_t *>(val.ptr);
            int irq_count        = static_cast<int>(val.len / bytes_per_irq);
            for (int i = 0; i < irq_count; i++) {
                push_irq_desc(interrupts, phandle, &cells[i * irq_cells],
                              irq_cells);
            }
        }

        static void parse_properties(const FDTNodeDesc& node,
                                     std::unordered_map<std::string, RawProperty>& properties)
        {
            FDTPropDesc prop;
            fdt_for_each_property_offset(prop, FDTHelper::fdt, node) {
                const char *prop_name = nullptr;
                int len               = 0;
                const void *prop_value =
                    fdt_getprop_by_offset(FDTHelper::fdt, prop, &prop_name,
                                          &len);
                if (prop_name == nullptr || len < 0) {
                    loggers::DEVICE::ERROR("节点 %s 属性读取失败: offset=%d",
                                           node_name(node), prop);
                    continue;
                }

                RawProperty raw_prop;
                if (prop_value != nullptr && len > 0) {
                    const byte *bytes = static_cast<const byte *>(prop_value);
                    for (int i = 0; i < len; i++) {
                        raw_prop.raw.push_back(bytes[i]);
                    }
                }
                properties[std::string(prop_name)] = raw_prop;
            }
        }

        static void parse_node(const FDTNodeDesc& node, RawDeviceConfig& config)
        {
            config                 = RawDeviceConfig();
            config.node_name       = std::string(node_name(node));
            config.default_phandle = parse_default_phandle(node);

            loggers::DEVICE::DEBUG("开始解析设备树节点: %s",
                                   config.node_name.c_str());

            parse_compatibles(node, config.compatibles);
            parse_regions(node, config.regions);
            parse_interrupts(node, config.interrupts);
            parse_properties(node, config.properties);

            loggers::DEVICE::DEBUG(
                "完成解析节点 %s: compatibles=%u regions=%u interrupts=%u properties=%u phandle=%u",
                config.node_name.c_str(),
                static_cast<unsigned int>(config.compatibles.size()),
                static_cast<unsigned int>(config.regions.size()),
                static_cast<unsigned int>(config.interrupts.size()),
                static_cast<unsigned int>(config.properties.size()),
                config.default_phandle);
        }

        static void parse(const void* dtb, RawConfiguration& config)
        {
            config.compatibles.clear();
            config.regionals.clear();
            config.devices.clear();
            if (dtb == nullptr) {
                loggers::DEVICE::ERROR("解析设备树失败: DTB 指针为空");
                return;
            }

            if (FDTHelper::fdt_init(const_cast<void *>(dtb)) == nullptr) {
                loggers::DEVICE::ERROR("解析设备树失败: %s",
                                       fdt_strerror(FDTHelper::errno));
                return;
            }

            int depth = 0;
            for (FDTNodeDesc node = fdt_next_node(FDTHelper::fdt, -1, &depth);
                 node >= 0;
                 node = fdt_next_node(FDTHelper::fdt, node, &depth))
            {
                if (!is_device_node(node)) {
                    continue;
                }

                RawDeviceConfig device;
                parse_node(node, device);
                if (!device.interrupts.empty()) {
                    loggers::DEVICE::DEBUG("节点 %s 归类为 device",
                                           device.node_name.c_str());
                    config.devices.push_back(device);
                } else if (!device.regions.empty()) {
                    loggers::DEVICE::DEBUG("节点 %s 归类为 regional",
                                           device.node_name.c_str());
                    config.regionals.push_back(as_regional_config(device));
                } else if (!device.compatibles.empty()) {
                    loggers::DEVICE::DEBUG("节点 %s 归类为 compatible",
                                           device.node_name.c_str());
                    config.compatibles.push_back(as_compatible_config(device));
                } else {
                    loggers::DEVICE::DEBUG("节点 %s 无可用设备配置, 跳过",
                                           device.node_name.c_str());
                }
            }

            loggers::DEVICE::DEBUG(
                "设备树解析完成, compatibles=%u regionals=%u devices=%u",
                static_cast<unsigned int>(config.compatibles.size()),
                static_cast<unsigned int>(config.regionals.size()),
                static_cast<unsigned int>(config.devices.size()));
        }
    };
}  // namespace

void parse_device_tree(const void* dtb, RawConfiguration& config) {
    DeviceTreeParser::parse(dtb, config);
}
