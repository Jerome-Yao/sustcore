/**
 * @file fdt.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 设备提供器实现
 * @version alpha-1.0.0
 * @date 2026-05-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/device/fdt_helper.h>
#include <device/fdt.h>
#include <device/model.h>
#include <libfdt.h>
#include <logger.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace {
    constexpr const char *PHANDLE_PROP          = "phandle";
    constexpr const char *LINUX_PHANDLE_PROP    = "linux,phandle";
    constexpr const char *REG_PROP              = "reg";
    constexpr const char *ADDRESS_CELLS_PROP    = "#address-cells";
    constexpr const char *SIZE_CELLS_PROP       = "#size-cells";
    constexpr const char *NO_MAP_PROP           = "no-map";
    constexpr const char *STATUS_PROP           = "status";
    constexpr const char *DEVICE_TYPE_PROP      = "device_type";
    constexpr const char *TIMEBASE_FREQ_PROP    = "timebase-frequency";
    constexpr const char *MODEL_PROP            = "model";
    constexpr const char *MMU_TYPE_PROP         = "mmu-type";
    constexpr const char *RISCV_ISA_PROP        = "riscv,isa";
    constexpr const char *CPU_PROP              = "cpu";
    constexpr const char *INTERRUPTS_PROP       = "interrupts";
    constexpr const char *INTERRUPT_EXT_PROP    = "interrupts-extended";
    constexpr const char *INTERRUPT_PARENT_PROP = "interrupt-parent";
    constexpr const char *INTC_PROP             = "interrupt-controller";
    constexpr const char *INTERRUPT_CELLS_PROP  = "#interrupt-cells";
    constexpr const char *CPU_MAP_NODE          = "cpu-map";
    constexpr const char *RISCV_NDEV_PROP       = "riscv,ndev";

    constexpr const char *OKAY_STATUS        = "okay";
    constexpr const char *MEMORY_DEVICE_TYPE = "memory";
    constexpr const char *CPU_DEVICE_TYPE    = "cpu";
    constexpr const char *CLINT_COMPATIBLE   = "riscv,clint0";
    constexpr const char *PLIC_COMPATIBLE    = "riscv,plic0";
    constexpr size_t CLINT_MAX_HW_IRQ        = 16;
    constexpr size_t MAX_PLIC_IRQS           = 256;

    constexpr const char *RESERVED_MEMORY_PATH = "/reserved-memory";
    constexpr const char *CPUS_PATH            = "/cpus";

    struct ParsedCpu {
        device::cpuid_t id;
        std::string model;
        std::string isa_string;
        std::string mmu_type;
        fdt::phandle_t cpu_phandle;
        fdt::phandle_t local_intc_phandle;
        fdt::phandle_t cpu_intc_phandle;
    };

    struct ClintBackendDescriptor {
        bool found                = false;
        device::intc_t identifier = device::INVALID_ICTRL_ID;
        std::string name;
        const fdt::Node *node = nullptr;
        std::vector<device::cpuid_t> target_harts;
    };

    struct PlicBackendDescriptor {
        bool found                = false;
        device::intc_t identifier = device::INVALID_ICTRL_ID;
        std::string name;
        const fdt::Node *node = nullptr;
        device::hwirq_t source_count = 0;
        std::vector<device::PlicContext> contexts;
    };

    /**
     * @brief 本地中断端点 phandle 到目标 hart 的映射.
     */
    using LocalInterruptTargetMap =
        std::unordered_map<fdt::phandle_t, device::cpuid_t>;

    /**
     * @brief 将 hart 列表排序并去重.
     *
     * @param target_harts 待规范化的 hart 列表.
     */
    void normalize_target_harts(
        std::vector<device::cpuid_t> &target_harts) noexcept {
        std::ranges::sort(target_harts);
        target_harts.erase(
            std::ranges::unique(target_harts,
                                [](device::cpuid_t lhs, device::cpuid_t rhs) {
                                    return lhs == rhs;
                                }),
            target_harts.end());
    }

    /**
     * @brief 判断节点状态是否为启用.
     */
    [[nodiscard]]
    bool node_status_enabled(const fdt::Node &node) {
        auto it = node.properties.find(STATUS_PROP);
        if (it == node.properties.end()) {
            return true;
        }

        std::string status = it->second->as_string();
        return status == fdt::OKAY_STATUS || status == fdt::OK_STATUS;
    }

    /**
     * @brief 获取节点 reg 属性的地址/长度 cell 配置.
     */
    [[nodiscard]]
    fdt::RegionCells node_region_cells(const fdt::Node &node) {
        size_t addr_cells = 2;
        size_t size_cells = 2;

        if (node.parent != nullptr) {
            auto addr_it = node.parent->properties.find(ADDRESS_CELLS_PROP);
            if (addr_it != node.parent->properties.end()) {
                addr_cells =
                    static_cast<size_t>(addr_it->second->as_integral());
            }

            auto size_it = node.parent->properties.find(SIZE_CELLS_PROP);
            if (size_it != node.parent->properties.end()) {
                size_cells =
                    static_cast<size_t>(size_it->second->as_integral());
            }
        }

        return {.addr_cells = addr_cells, .size_cells = size_cells};
    }

    /**
     * @brief 递归构建设备树节点对象.
     */
    void build_node_recursive(const void *dtb, fdt::Configuration &config,
                              fdt::Node &node, int node_offset) {
        int prop_offset;
        fdt_for_each_property_offset(prop_offset, dtb, node_offset) {
            const char *prop_name = nullptr;
            int prop_len          = 0;
            const void *prop_data =
                fdt_getprop_by_offset(dtb, prop_offset, &prop_name, &prop_len);
            if (prop_name == nullptr || prop_data == nullptr || prop_len < 0) {
                continue;
            }

            auto *prop = new fdt::Property{
                .name = prop_name,
                .data = static_cast<const char *>(prop_data),
                .size = static_cast<size_t>(prop_len),
            };
            node.properties[prop->name] = util::owner(prop);

            if ((prop->name == PHANDLE_PROP ||
                 prop->name == LINUX_PHANDLE_PROP) &&
                node.phandle == 0)
            {
                node.phandle = prop->as_phandle();
            }
        }

        if (node.phandle != 0) {
            config.phandle_map[node.phandle] = &node;
        }

        int child_offset;
        fdt_for_each_subnode(child_offset, dtb, node_offset) {
            const char *child_name = fdt_get_name(dtb, child_offset, nullptr);
            if (child_name == nullptr) {
                continue;
            }

            auto child    = util::owner(new fdt::Node());
            child->name   = child_name;
            child->parent = &node;

            build_node_recursive(dtb, config, *child, child_offset);
            node.children[child->name] = std::move(child);
        }
    }

    /**
     * @brief 判断节点是否存在指定属性.
     */
    [[nodiscard]]
    bool has_property(const fdt::Node &node, const char *prop_name) {
        return node.properties.contains(prop_name);
    }

    /**
     * @brief 判断字符串属性是否等于给定值.
     */
    [[nodiscard]]
    bool is_string_prop_equal(const fdt::Node &node, const char *prop_name,
                              const char *expected) {
        auto it = node.properties.find(prop_name);
        if (it == node.properties.end()) {
            return false;
        }
        return it->second->as_string() == expected;
    }

    /**
     * @brief 判断 compatible 列表中是否包含目标兼容串.
     */
    [[nodiscard]]
    bool compatible_contains(const fdt::Node &node,
                             const char *expected) noexcept {
        auto it = node.properties.find(fdt::COMPATIBLE_PROP);
        if (it == node.properties.end()) {
            return false;
        }

        for (const auto &compatible : it->second->as_string_list()) {
            if (compatible == expected) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 解析节点的 reg 单整数值.
     */
    [[nodiscard]]
    std::optional<uint64_t> parse_reg_value(const fdt::Node &node) noexcept {
        auto reg_it = node.properties.find(REG_PROP);
        if (reg_it == node.properties.end()) {
            return std::nullopt;
        }
        return reg_it->second->as_integral();
    }

    /**
     * @brief 按名称排序返回子节点列表.
     */
    [[nodiscard]]
    std::vector<const fdt::Node *> sorted_children(const fdt::Node &node) {
        std::vector<const fdt::Node *> result;
        result.reserve(node.children.size());
        for (const auto &[_, child] : node.children) {
            result.push_back(child.get());
        }
        std::ranges::sort(result,
                          [](const fdt::Node *lhs, const fdt::Node *rhs) {
                              return lhs->name < rhs->name;
                          });
        return result;
    }

    /**
     * @brief 为 CPU 节点推导可读型号字符串.
     */
    [[nodiscard]]
    std::string fallback_cpu_model(const fdt::Node &node) {
        auto model_it = node.properties.find(MODEL_PROP);
        if (model_it != node.properties.end()) {
            std::string model = model_it->second->as_string();
            if (!model.empty()) {
                return model;
            }
        }

        auto compatible_it = node.properties.find(fdt::COMPATIBLE_PROP);
        if (compatible_it != node.properties.end()) {
            auto compatibles = compatible_it->second->as_string_list();
            if (!compatibles.empty()) {
                return compatibles.front();
            }
        }

        return node.name;
    }

    /**
     * @brief 提取 CPU 节点下本地中断控制器的 phandle.
     */
    [[nodiscard]]
    std::optional<fdt::phandle_t> find_local_intc_phandle(
        const fdt::Node &cpu_node) noexcept {
        for (const auto &[_, child] : cpu_node.children) {
            if (!has_property(*child, INTC_PROP)) {
                continue;
            }
            if (child->phandle != 0) {
                return child->phandle;
            }
            loggers::DEVICE::WARN(
                "CPU 节点 %s 的 interrupt-controller 缺少 phandle",
                cpu_node.name.c_str());
            return std::nullopt;
        }
        return std::nullopt;
    }

    /**
     * @brief 将属性数据解析为 u32 cell 数组.
     */
    [[nodiscard]]
    std::vector<uint32_t> parse_u32_cells(const fdt::Property &prop) {
        constexpr size_t CELL_SIZE = sizeof(uint32_t);
        if (prop.size % CELL_SIZE != 0) {
            return {};
        }

        std::vector<uint32_t> cells;
        cells.reserve(prop.size / CELL_SIZE);
        for (size_t offset = 0; offset < prop.size; offset += CELL_SIZE) {
            uint64_t value = 0;
            if (!fdt::detail::parse_be_integer(prop.data + offset, CELL_SIZE,
                                               value))
            {
                return {};
            }
            cells.push_back(static_cast<uint32_t>(value));
        }
        return cells;
    }

    /**
     * @brief 从中断引用列表中提取目标 hart 集合.
     *
     * @param node_name 当前设备节点名称, 仅用于日志.
     * @param refs 中断引用列表.
     * @param local_intc_map 本地中断端点到 hart 的映射.
     * @return std::vector<device::cpuid_t> 去重后的目标 hart 列表.
     */
    [[nodiscard]]
    std::vector<device::cpuid_t> target_harts_from_interrupt_refs(
        const char *node_name,
        const std::vector<std::pair<fdt::phandle_t, device::hwirq_t>> &refs,
        const LocalInterruptTargetMap &local_intc_map) {
        std::vector<device::cpuid_t> target_harts;
        target_harts.reserve(refs.size());

        for (const auto &[phandle, hwirq] : refs) {
            auto cpu_it = local_intc_map.find(phandle);
            if (cpu_it == local_intc_map.end()) {
                loggers::DEVICE::DEBUG(
                    "%s 的中断引用未匹配到本地 hart: phandle=%u hwirq=%u",
                    node_name, phandle, static_cast<unsigned>(hwirq));
                continue;
            }
            target_harts.push_back(cpu_it->second);
        }

        normalize_target_harts(target_harts);
        return target_harts;
    }

    /**
     * @brief 将 cpu-map 节点名映射为拓扑层级.
     */
    [[nodiscard]]
    std::optional<device::CpuTopoLevel> topo_level_from_name(
        std::string_view name) noexcept {
        if (name.starts_with("thread")) {
            return device::CpuTopoLevel::THREAD;
        }
        if (name.starts_with("core")) {
            return device::CpuTopoLevel::CORE;
        }
        if (name.starts_with("cluster")) {
            return device::CpuTopoLevel::CLUSTER;
        }
        if (name.starts_with("package") || name.starts_with("socket")) {
            return device::CpuTopoLevel::PACKAGE;
        }
        if (name.starts_with("numa")) {
            return device::CpuTopoLevel::NUMA;
        }
        return std::nullopt;
    }

    /**
     * @brief 递归构建 cpu-map 子树.
     */
    [[nodiscard]]
    Result<std::vector<device::cpuid_t>> build_cpu_map_subtree(
        device::CpuTopologyBuilder &builder, const fdt::Node &map_node,
        device::topo_t parent_id, device::topo_t &next_topo_id,
        const std::unordered_map<fdt::phandle_t, device::cpuid_t>
            &cpu_phandle_map) {
        // 识别当前拓扑层级.
        auto level = topo_level_from_name(map_node.name);
        if (!level.has_value()) {
            loggers::DEVICE::WARN("忽略未知 cpu-map 拓扑节点: %s",
                                  map_node.name.c_str());
            return std::vector<device::cpuid_t>{};
        }

        device::topo_t node_id = next_topo_id++;
        auto add_res           = builder.add_child(parent_id, *level, node_id);
        if (!add_res.has_value()) {
            propagate_return(add_res);
        }

        // 收集当前节点直接声明的 CPU.
        std::vector<device::cpuid_t> aggregated;
        auto cpu_it = map_node.properties.find(CPU_PROP);
        if (cpu_it != map_node.properties.end()) {
            fdt::phandle_t cpu_phandle = cpu_it->second->as_phandle();
            auto cpu_map_it            = cpu_phandle_map.find(cpu_phandle);
            if (cpu_map_it == cpu_phandle_map.end()) {
                loggers::DEVICE::ERROR(
                    "cpu-map 节点 %s 引用了未知 CPU phandle=%u",
                    map_node.name.c_str(), cpu_phandle);
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            aggregated.push_back(cpu_map_it->second);
        }

        // 递归构建子节点.
        for (const auto *child : sorted_children(map_node)) {
            auto child_res = build_cpu_map_subtree(
                builder, *child, node_id, next_topo_id, cpu_phandle_map);
            propagate(child_res);
            const auto &child_cpus = child_res.value();
            aggregated.insert(aggregated.end(), child_cpus.begin(),
                              child_cpus.end());
        }

        // 规范化 CPU 列表并回填到节点.
        std::ranges::sort(aggregated);
        aggregated.erase(
            std::ranges::unique(aggregated,
                                [](device::cpuid_t lhs, device::cpuid_t rhs) {
                                    return lhs == rhs;
                                }),
            aggregated.end());

        auto cpus_res = builder.cpus(node_id, aggregated);
        propagate(cpus_res);
        return aggregated;
    }

    /**
     * @brief 构造保底 CPU 拓扑.
     */
    [[nodiscard]]
    Result<device::CpuTopology> build_default_topology(
        const std::vector<device::cpuid_t> &cpu_ids) {
        device::CpuTopologyBuilder builder;

        // 构造根 cluster.
        builder.root(device::CpuTopoLevel::CLUSTER, 0);

        auto root_res = builder.cpus(0, cpu_ids);
        propagate(root_res);

        // 为每个 CPU 创建独立 core 节点.
        device::topo_t next_id = 1;
        for (device::cpuid_t cpu_id : cpu_ids) {
            auto add_res =
                builder.add_child(0, device::CpuTopoLevel::CORE, next_id);
            propagate(add_res);
            auto cpu_res = builder.cpus(next_id, {cpu_id});
            propagate(cpu_res);
            ++next_id;
        }

        return builder.build();
    }

    /**
     * @brief 从 cpu-map 构建 CPU 拓扑.
     */
    [[nodiscard]]
    Result<device::CpuTopology> build_cpu_map_topology(
        const fdt::Node &cpu_map_node,
        const std::unordered_map<fdt::phandle_t, device::cpuid_t>
            &cpu_phandle_map,
        const std::vector<device::cpuid_t> &all_cpu_ids) {
        // 收集顶层拓扑节点.
        auto top_children = sorted_children(cpu_map_node);
        if (top_children.empty()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        device::CpuTopologyBuilder builder;
        device::topo_t next_topo_id = 1;

        // 单根拓扑直接将该节点提升为树根.
        if (top_children.size() == 1) {
            auto root_level = topo_level_from_name(top_children.front()->name);
            if (!root_level.has_value()) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            builder.root(*root_level, 0);

            std::vector<device::cpuid_t> aggregated;
            auto cpu_it = top_children.front()->properties.find(CPU_PROP);
            if (cpu_it != top_children.front()->properties.end()) {
                fdt::phandle_t cpu_phandle = cpu_it->second->as_phandle();
                auto cpu_map_it            = cpu_phandle_map.find(cpu_phandle);
                if (cpu_map_it == cpu_phandle_map.end()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                aggregated.push_back(cpu_map_it->second);
            }

            for (const auto *child : sorted_children(*top_children.front())) {
                auto child_res = build_cpu_map_subtree(
                    builder, *child, 0, next_topo_id, cpu_phandle_map);
                propagate(child_res);
                const auto &child_cpus = child_res.value();
                aggregated.insert(aggregated.end(), child_cpus.begin(),
                                  child_cpus.end());
            }

            std::ranges::sort(aggregated);
            aggregated.erase(
                std::ranges::unique(
                    aggregated, [](device::cpuid_t lhs,
                                   device::cpuid_t rhs) { return lhs == rhs; }),
                aggregated.end());
            auto root_res = builder.cpus(0, aggregated);
            propagate(root_res);
        } else {
            // 多个顶层节点时，额外插入一个 package 根节点.
            builder.root(device::CpuTopoLevel::PACKAGE, 0);
            auto root_res = builder.cpus(0, all_cpu_ids);
            propagate(root_res);

            for (const auto *child : top_children) {
                auto child_res = build_cpu_map_subtree(
                    builder, *child, 0, next_topo_id, cpu_phandle_map);
                propagate(child_res);
            }
        }

        return builder.build();
    }

    /**
     * @brief 解析单个 CPU 节点.
     */
    [[nodiscard]]
    Result<ParsedCpu> parse_cpu_node(const fdt::Node &cpu_node) {
        // 基本状态与类型检查.
        if (!node_status_enabled(cpu_node)) {
            unexpect_return(ErrCode::BUSY);
        }
        if (!is_string_prop_equal(cpu_node, DEVICE_TYPE_PROP, CPU_DEVICE_TYPE))
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        // 解析 CPU 编号与 phandle.
        auto reg_value = parse_reg_value(cpu_node);
        if (!reg_value.has_value()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少 reg 属性",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (*reg_value > std::numeric_limits<device::cpuid_t>::max()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 的 reg 超出 cpuid 范围: %llu",
                                   cpu_node.name.c_str(),
                                   static_cast<unsigned long long>(*reg_value));
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        if (cpu_node.phandle == 0) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少 phandle",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        // 解析 ISA 与本地中断控制器.
        auto isa_it = cpu_node.properties.find(RISCV_ISA_PROP);
        if (isa_it == cpu_node.properties.end()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少 riscv,isa 属性",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto local_intc = find_local_intc_phandle(cpu_node);
        if (!local_intc.has_value()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少本地 interrupt-controller",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        ParsedCpu parsed{
            .id                 = static_cast<device::cpuid_t>(*reg_value),
            .model              = fallback_cpu_model(cpu_node),
            .isa_string         = isa_it->second->as_string(),
            .mmu_type           = cpu_node.properties.contains(MMU_TYPE_PROP)
                                      ? cpu_node.properties.at(MMU_TYPE_PROP)->as_string()
                                      : "",
            .cpu_phandle        = cpu_node.phandle,
            .local_intc_phandle = *local_intc,
            .cpu_intc_phandle   = *local_intc,
        };
        return parsed;
    }

    /**
     * @brief 扫描并提取 CLINT 描述信息.
     */
    void find_clint_recursive(const fdt::Node &node,
                              ClintBackendDescriptor &descriptor) {
        if (!descriptor.found && compatible_contains(node, CLINT_COMPATIBLE)) {
            descriptor.found = true;
            descriptor.node  = &node;
            descriptor.identifier =
                node.phandle != 0 ? static_cast<device::intc_t>(node.phandle)
                                  : static_cast<device::intc_t>(1);
            descriptor.name = node.name;
            return;
        }

        for (const auto &[_, child] : node.children) {
            find_clint_recursive(*child, descriptor);
            if (descriptor.found) {
                return;
            }
        }
    }

    /**
     * @brief 递归扫描并提取 PLIC 描述信息.
     */
    void find_plic_recursive(const fdt::Node &node,
                             PlicBackendDescriptor &descriptor) {
        if (!descriptor.found && compatible_contains(node, PLIC_COMPATIBLE)) {
            descriptor.found = true;
            descriptor.node  = &node;
            descriptor.identifier =
                node.phandle != 0 ? static_cast<device::intc_t>(node.phandle)
                                  : static_cast<device::intc_t>(3);
            descriptor.name = node.name;

            auto ndev_it = node.properties.find(RISCV_NDEV_PROP);
            if (ndev_it != node.properties.end()) {
                descriptor.source_count =
                    static_cast<device::hwirq_t>(ndev_it->second->as_integral());
            } else {
                loggers::DEVICE::WARN("PLIC 节点 %s 缺少 riscv,ndev 属性",
                                      node.name.c_str());
            }
            return;
        }

        for (const auto &[_, child] : node.children) {
            find_plic_recursive(*child, descriptor);
            if (descriptor.found) {
                return;
            }
        }
    }
}  // namespace

namespace fdt {
    /**
     * @brief 使用 FDT 上下文构造统一设备节点包装器.
     */
    FDTDeviceNode::FDTDeviceNode(const FDTProvider &provider,
                                 const Configuration &config,
                                 const Node &node) noexcept
        : _provider(&provider),
          _config(&config),
          _node(&node) {}

    /**
     * @brief 获取节点所属平台名称.
     */
    const char *FDTDeviceNode::platform() const noexcept {
        return "fdt";
    }

    /**
     * @brief 查询统一语义下的节点属性.
     */
    Optional<device::DevicePropView> FDTDeviceNode::property(
        const std::string &name) const {
        if (_node == nullptr || _provider == nullptr || _config == nullptr) {
            return std::nullopt;
        }

        if (name == device::STANDARD_COMPATIBLE_KEY) {
            return raw_property(COMPATIBLE_PROP);
        }
        if (name == device::STANDARD_MMIO_KEY) {
            return mmio_property();
        }
        if (name == device::STANDARD_IRQ_KEY) {
            return irq_property();
        }
        if (name == device::STANDARD_INTERRUPT_PARENT_KEY) {
            return interrupt_parent_property();
        }
        return raw_property(name.c_str());
    }

    /**
     * @brief 读取原始 FDT 属性并包装为统一属性视图.
     */
    Optional<device::DevicePropView> FDTDeviceNode::raw_property(
        const char *prop_name) const noexcept {
        if (_node == nullptr || prop_name == nullptr) {
            return std::nullopt;
        }

        auto prop_it = _node->properties.find(prop_name);
        if (prop_it == _node->properties.end()) {
            return std::nullopt;
        }

        const auto &prop = *prop_it->second;
        device::DevicePropView::PropType type =
            device::DevicePropView::PropType::ANY;
        if (prop.size == 0) {
            type = device::DevicePropView::PropType::NONE;
        } else if (std::strcmp(prop_name, COMPATIBLE_PROP) == 0 ||
                   std::strcmp(prop_name, STATUS_PROP) == 0 ||
                   std::strcmp(prop_name, DEVICE_TYPE_PROP) == 0)
        {
            auto strings = prop.as_string_list();
            type = strings.size() <= 1
                       ? device::DevicePropView::PropType::STRING
                       : device::DevicePropView::PropType::STRING_LIST;
        } else if (prop.size == sizeof(sus_u32)) {
            type = device::DevicePropView::PropType::INTEGER;
        } else if (prop.size % sizeof(sus_u32) == 0) {
            type = device::DevicePropView::PropType::INTEGER_LIST;
        } else {
            type = device::DevicePropView::PropType::BYTE_ARRAY;
        }

        return device::DevicePropView(
            type, reinterpret_cast<const byte *>(prop.data), prop.size);
    }

    /**
     * @brief 解析当前节点的 MMIO 区域列表.
     */
    Optional<device::DevicePropView> FDTDeviceNode::mmio_property()
        const noexcept {
        if (_node == nullptr) {
            return std::nullopt;
        }

        auto reg_it = _node->properties.find(REG_PROP);
        if (reg_it == _node->properties.end()) {
            loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 缺少 reg 属性",
                                   _node->name.c_str());
            return std::nullopt;
        }

        auto regions = reg_it->second->as_regions(node_region_cells(*_node));
        loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 解析 mmio 区域数=%u",
                               _node->name.c_str(),
                               static_cast<unsigned>(regions.size()));
        return device::DevicePropView::from_region_list(std::move(regions));
    }

    /**
     * @brief 解析当前节点的统一 IRQ 列表.
     */
    Optional<device::DevicePropView> FDTDeviceNode::irq_property()
        const noexcept {
        if (_node == nullptr || !device::DeviceModel::initialized()) {
            return std::nullopt;
        }

        auto &irqman = device::DeviceModel::inst().interrupt();
        auto virqs_res = _provider->parse_interrupt_virqs(*_node, irqman);
        if (!virqs_res.has_value()) {
            if (virqs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                loggers::DEVICE::ERROR("FDTDeviceNode[%s] 解析 irqs 失败: %s",
                                       _node->name.c_str(),
                                       to_cstring(virqs_res.error()));
            } else {
                loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 未声明 irqs",
                                       _node->name.c_str());
            }
            return std::nullopt;
        }

        loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 解析 irqs 数量=%u",
                               _node->name.c_str(),
                               static_cast<unsigned>(virqs_res.value().size()));
        return device::DevicePropView::from_virq_list(
            std::move(virqs_res.value()));
    }

    /**
     * @brief 解析当前节点的中断父节点标识.
     */
    Optional<device::DevicePropView> FDTDeviceNode::interrupt_parent_property()
        const noexcept {
        if (_node == nullptr) {
            return std::nullopt;
        }

        auto parent_res = _provider->resolve_interrupt_parent(*_node);
        if (!parent_res.has_value()) {
            if (parent_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                loggers::DEVICE::ERROR(
                    "FDTDeviceNode[%s] 解析 interrupt-parent 失败: %s",
                    _node->name.c_str(), to_cstring(parent_res.error()));
            } else {
                loggers::DEVICE::DEBUG(
                    "FDTDeviceNode[%s] 未声明 interrupt-parent",
                    _node->name.c_str());
            }
            return std::nullopt;
        }

        Node *parent_node = _config->get_node_by_phandle(parent_res.value());
        if (parent_node == nullptr) {
            loggers::DEVICE::ERROR(
                "FDTDeviceNode[%s] interrupt-parent phandle=%u 不存在",
                _node->name.c_str(), parent_res.value());
            return std::nullopt;
        }

        auto prop_it = parent_node->properties.find(PHANDLE_PROP);
        if (prop_it == parent_node->properties.end()) {
            prop_it = parent_node->properties.find(LINUX_PHANDLE_PROP);
        }
        if (prop_it == parent_node->properties.end()) {
            loggers::DEVICE::ERROR(
                "FDTDeviceNode[%s] interrupt-parent 节点 %s 缺少 phandle 属性",
                _node->name.c_str(), parent_node->name.c_str());
            return std::nullopt;
        }

        return device::DevicePropView(
            device::DevicePropView::PropType::INTEGER,
            reinterpret_cast<const byte *>(prop_it->second->data),
            prop_it->second->size);
    }

    Node *Configuration::get_node_by_path(const std::string &path) const {
        if (!root || root.get() == nullptr) {
            return nullptr;
        }
        if (path.empty() || path == "/") {
            return root.get();
        }

        Node *current    = root.get();
        size_t component = 0;
        while (component < path.size()) {
            while (component < path.size() && path[component] == '/') {
                ++component;
            }
            if (component >= path.size()) {
                break;
            }

            size_t end = component;
            while (end < path.size() && path[end] != '/') {
                ++end;
            }

            std::string name = path.substr(component, end - component);
            auto child_it    = current->children.find(name);
            if (child_it == current->children.end()) {
                return nullptr;
            }
            current   = child_it->second.get();
            component = end;
        }
        return current;
    }

    Node *Configuration::get_node_by_phandle(phandle_t phandle) const {
        auto it = phandle_map.find(phandle);
        return it == phandle_map.end() ? nullptr : it->second;
    }

    Result<void> Configuration::add_node(Node *parent,
                                         util::owner<Node *> new_node) {
        if (new_node.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        if (!root) {
            if (parent != nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            root         = std::move(new_node);
            root->parent = nullptr;
            if (root->phandle != 0) {
                phandle_map[root->phandle] = root.get();
            }
            void_return();
        }

        if (parent == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        new_node->parent = parent;
        if (parent->children.contains(new_node->name)) {
            unexpect_return(ErrCode::BUSY);
        }

        if (new_node->phandle != 0) {
            phandle_map[new_node->phandle] = new_node.get();
        }
        parent->children[new_node->name] = std::move(new_node);
        void_return();
    }

    Result<void> Configuration::remove_node(Node *node) {
        if (node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (!root || root.get() == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (node == root.get()) {
            if (root->phandle != 0) {
                phandle_map.erase(root->phandle);
            }
            root = util::owner<Node *>(nullptr);
            void_return();
        }

        Node *parent = node->parent;
        if (parent == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto it = parent->children.find(node->name);
        if (it == parent->children.end() || it->second.get() != node) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (node->phandle != 0) {
            phandle_map.erase(node->phandle);
        }
        parent->children.erase(it);
        void_return();
    }

    void make_config(void *dtb, Configuration &config) {
        config.root = util::owner<Node *>(nullptr);
        config.phandle_map.clear();

        if (dtb == nullptr) {
            assert(false && "dtb must not be null");
            return;
        }
        if (FDTHelper::fdt_init(dtb) == nullptr) {
            assert(false && "fdt_init failed");
            return;
        }

        auto root    = util::owner(new Node());
        root->name   = "/";
        root->parent = nullptr;
        build_node_recursive(static_cast<const void *>(FDTHelper::fdt), config,
                             *root, FDTHelper::get_root_node());
        config.root = std::move(root);
    }
}  // namespace fdt

namespace fdt {
    namespace {
        /**
         * @brief 将 CPU local external virq 重新分发到 PLIC 设备 virq.
         */
        class PlicDispatchAction {
        public:
            /**
             * @brief 构造一个 PLIC external virq 重分发动作.
             *
             * @param plic 目标 PLIC 后端.
             * @param domain_id PLIC 中断域 ID.
             */
            PlicDispatchAction(device::Plic &plic, device::domain_t domain_id) noexcept
                : _plic(plic), _domain_id(domain_id) {}

            /**
             * @brief 处理一次 CPU local external virq.
             *
             * @param event 当前 local external 中断事件.
             */
            void handle(const device::IrqEvent &event) noexcept {
                auto claim_res = _plic.resolve_claim_for_current_hart();
                if (!claim_res.has_value()) {
                    loggers::INTERRUPT::ERROR(
                        "PLIC claim 失败: domain=%u err=%s",
                        _domain_id, to_cstring(claim_res.error()));
                    return;
                }

                auto &irqman = device::DeviceModel::inst().interrupt();
                auto virq_res = irqman.allocate_virq(_domain_id, claim_res.value());
                if (!virq_res.has_value()) {
                    loggers::INTERRUPT::ERROR(
                        "PLIC 域解析 virq 失败: domain=%u hwirq=%u err=%s",
                        _domain_id, static_cast<unsigned>(claim_res.value()),
                        to_cstring(virq_res.error()));
                    return;
                }

                auto dispatch_res = irqman.dispatch(device::IrqEvent{
                    .virq    = virq_res.value(),
                    .hw_irq  = claim_res.value(),
                    .scause  = event.scause,
                    .sepc    = event.sepc,
                    .stval   = event.stval,
                    .context = event.context,
                });
                if (!dispatch_res.has_value()) {
                    loggers::INTERRUPT::ERROR(
                        "PLIC 二次分发失败: virq=%llu hwirq=%u err=%s",
                        static_cast<unsigned long long>(virq_res.value()),
                        static_cast<unsigned>(claim_res.value()),
                        to_cstring(dispatch_res.error()));
                }
            }

        private:
            device::Plic &_plic;
            device::domain_t _domain_id = device::INVALID_DOMAIN_ID;
        };
    }  // namespace

    Result<void> FDTProvider::register_irq_domain(
        phandle_t phandle, const device::IrqDomain &domain) const {
        if (phandle == 0) {
            loggers::DEVICE::ERROR("拒绝登记无效中断控制器 phandle=0");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto it = _irq_domains.find(phandle);
        if (it != _irq_domains.end()) {
            if (it->second != domain.id()) {
                loggers::DEVICE::ERROR(
                    "中断控制器 phandle=%u 已映射到 domain=%u, 无法改写为 "
                    "domain=%u",
                    phandle, it->second, domain.id());
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
            loggers::DEVICE::DEBUG(
                "中断控制器 phandle=%u 已登记到 domain=%u, 跳过重复登记",
                phandle, domain.id());
            void_return();
        }

        _irq_domains[phandle] = domain.id();
        loggers::DEVICE::DEBUG("登记中断控制器 phandle=%u -> domain=%u",
                               phandle, domain.id());
        void_return();
    }

    Result<device::IrqDomain &> FDTProvider::resolve_irq_domain(
        phandle_t phandle, device::IrqManager &irqman) const {
        auto it = _irq_domains.find(phandle);
        if (it == _irq_domains.end()) {
            loggers::DEVICE::ERROR("未找到 phandle=%u 对应的中断域映射",
                                   phandle);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        loggers::DEVICE::DEBUG("解析中断域: phandle=%u -> domain=%u", phandle,
                               it->second);
        return irqman.get_domain(it->second);
    }

    Result<size_t> FDTProvider::interrupt_cells_for_controller(
        phandle_t controller_phandle) const {
        Node *controller = _config.get_node_by_phandle(controller_phandle);
        if (controller == nullptr) {
            loggers::DEVICE::ERROR("找不到 phandle=%u 对应的中断控制器节点",
                                   controller_phandle);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto cells_it = controller->properties.find(INTERRUPT_CELLS_PROP);
        if (cells_it == controller->properties.end()) {
            loggers::DEVICE::ERROR("中断控制器节点 %s 缺少 %s",
                                   controller->name.c_str(),
                                   INTERRUPT_CELLS_PROP);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        size_t cell_count =
            static_cast<size_t>(cells_it->second->as_integral());
        if (cell_count != 1) {
            loggers::DEVICE::ERROR(
                "中断控制器节点 %s 的 %s=%u, 当前仅支持单 cell 中断编码",
                controller->name.c_str(), INTERRUPT_CELLS_PROP,
                static_cast<unsigned>(cell_count));
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        return cell_count;
    }

    Result<phandle_t> FDTProvider::resolve_interrupt_parent(
        const Node &node) const {
        for (const Node *current = &node; current != nullptr;
             current             = current->parent)
        {
            auto parent_it = current->properties.find(INTERRUPT_PARENT_PROP);
            if (parent_it == current->properties.end()) {
                continue;
            }

            phandle_t phandle = parent_it->second->as_phandle();
            if (phandle == 0) {
                loggers::DEVICE::ERROR("节点 %s 的 interrupt-parent 为 0",
                                       current->name.c_str());
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return phandle;
        }

        loggers::DEVICE::ERROR("节点 %s 及其祖先均未声明 interrupt-parent",
                               node.name.c_str());
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<std::vector<FDTProvider::InterruptRef>>
    FDTProvider::parse_interrupts_extended(const Node &node) const {
        auto prop_it = node.properties.find(INTERRUPT_EXT_PROP);
        if (prop_it == node.properties.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto cells = parse_u32_cells(*prop_it->second);
        if (cells.empty()) {
            loggers::DEVICE::ERROR("节点 %s 的 interrupts-extended 为空或非法",
                                   node.name.c_str());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        std::vector<InterruptRef> refs;
        refs.reserve(cells.size() / 2);

        for (size_t offset = 0; offset < cells.size();) {
            phandle_t phandle = cells[offset++];
            if (phandle == 0) {
                loggers::DEVICE::ERROR(
                    "节点 %s 的 interrupts-extended 含有 phandle=0",
                    node.name.c_str());
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            auto cell_count_res = interrupt_cells_for_controller(phandle);
            propagate(cell_count_res);
            size_t cell_count = cell_count_res.value();
            if (offset + cell_count > cells.size()) {
                loggers::DEVICE::ERROR(
                    "节点 %s 的 interrupts-extended 长度不足, phandle=%u "
                    "需要 %u 个中断 cell",
                    node.name.c_str(), phandle,
                    static_cast<unsigned>(cell_count));
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            device::hwirq_t hwirq = static_cast<device::hwirq_t>(cells[offset]);
            offset += cell_count;

            refs.emplace_back(phandle, hwirq);
            loggers::DEVICE::DEBUG(
                "解析 interrupts-extended: node=%s phandle=%u hwirq=%u",
                node.name.c_str(), phandle, static_cast<unsigned>(hwirq));
        }

        return refs;
    }

    Result<std::vector<FDTProvider::InterruptRef>>
    FDTProvider::parse_interrupts(const Node &node) const {
        auto parent_res = resolve_interrupt_parent(node);
        propagate(parent_res);
        phandle_t parent_phandle = parent_res.value();

        auto cell_count_res = interrupt_cells_for_controller(parent_phandle);
        propagate(cell_count_res);
        size_t cell_count = cell_count_res.value();

        auto prop_it = node.properties.find(INTERRUPTS_PROP);
        if (prop_it == node.properties.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto cells = parse_u32_cells(*prop_it->second);
        if (cells.empty() || cells.size() % cell_count != 0) {
            loggers::DEVICE::ERROR("节点 %s 的 interrupts 属性长度非法",
                                   node.name.c_str());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        std::vector<InterruptRef> refs;
        refs.reserve(cells.size() / cell_count);
        for (size_t offset = 0; offset < cells.size(); offset += cell_count) {
            device::hwirq_t hwirq = static_cast<device::hwirq_t>(cells[offset]);
            refs.emplace_back(parent_phandle, hwirq);
            loggers::DEVICE::DEBUG(
                "解析 interrupts: node=%s parent=%u hwirq=%u",
                node.name.c_str(), parent_phandle,
                static_cast<unsigned>(hwirq));
        }

        return refs;
    }

    Result<std::vector<device::virq_t>>
    FDTProvider::resolve_interrupt_refs_to_virqs(
        const std::vector<InterruptRef> &refs,
        device::IrqManager &irqman) const {
        std::vector<device::virq_t> virqs;
        virqs.reserve(refs.size());

        for (const auto &[phandle, hwirq] : refs) {
            auto domain_res = resolve_irq_domain(phandle, irqman);
            propagate(domain_res);

            auto virq_res =
                irqman.allocate_virq(domain_res.value().get().id(), hwirq);
            propagate(virq_res);

            virqs.push_back(virq_res.value());
            loggers::DEVICE::DEBUG(
                "解析中断引用到 virq: phandle=%u domain=%u hwirq=%u virq=%llu",
                phandle, domain_res.value().get().id(),
                static_cast<unsigned>(hwirq),
                static_cast<unsigned long long>(virq_res.value()));
        }

        return virqs;
    }

    Result<std::vector<device::virq_t>> FDTProvider::parse_interrupt_virqs(
        const Node &node, device::IrqManager &irqman) const {
        auto ext_refs_res = parse_interrupts_extended(node);
        if (ext_refs_res.has_value()) {
            return resolve_interrupt_refs_to_virqs(ext_refs_res.value(),
                                                   irqman);
        }
        if (ext_refs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
            propagate_return(ext_refs_res);
        }

        auto refs_res = parse_interrupts(node);
        propagate(refs_res);
        return resolve_interrupt_refs_to_virqs(refs_res.value(), irqman);
    }

    void FDTProvider::append_as_regions(
        std::vector<device::MemRegion> &regions, const RegionCells &cells,
        const Property &prop, device::MemRegion::MemoryStatus status) const {
        auto new_regions = prop.as_regions(cells);
        for (const auto &area : new_regions) {
            regions.emplace_back(area, status);
        }
    }

    bool FDTProvider::is_memory_node(Node &node) const {
        return is_string_prop_equal(node, DEVICE_TYPE_PROP, MEMORY_DEVICE_TYPE);
    }

    void FDTProvider::register_memory_regions(
        device::DeviceModel &model) const {
        if (!_config.root) {
            return;
        }

        std::vector<device::MemRegion> regions;

        // 加入所有 device_type = memory 的节点下的区域作为FREE区域
        for (const auto &[_, node] : _config.root->children) {
            if (!node_status_enabled(*node) || !is_memory_node(*node)) {
                continue;
            }
            auto reg_it = node->properties.find(REG_PROP);
            if (reg_it == node->properties.end()) {
                loggers::DEVICE::WARN("内存节点 /%s 缺少 reg 属性, 已跳过",
                                      node->name.c_str());
                continue;
            }
            append_as_regions(regions, node_region_cells(*node),
                              *reg_it->second,
                              device::MemRegion::MemoryStatus::FREE);
        }

        // 加入 reserved-memory 下的所有保留区域
        Node *reserved_memory = _config.get_node_by_path(RESERVED_MEMORY_PATH);
        if (reserved_memory != nullptr) {
            for (const auto &[_, child] : reserved_memory->children) {
                if (!node_status_enabled(*child)) {
                    loggers::DEVICE::WARN(
                        "节点 /reserved-memory/%s 缺少 reg 属性, 已跳过",
                        child->name.c_str());
                    continue;
                }
                append_as_regions(regions, node_region_cells(*child),
                                  *child->properties.at(REG_PROP),
                                  device::MemRegion::MemoryStatus::RESERVED);
            }
        }
        model.collect_memory_regions(&regions);
    }

    void FDTProvider::register_cpus(device::DeviceModel &model) const {
        auto &cpus = model.cpus();
        _irq_domains.clear();

        // 清理旧的 CPU 组信息.
        loggers::DEVICE::DEBUG("开始更新 CPU 组信息");
        cpus.cleanup();
        cpus.topology.cleanup();

        // 解析 /cpus 根节点与频率.
        if (!_config.root) {
            loggers::DEVICE::WARN("设备树根节点不存在, 无法获取 CPU 信息");
            return;
        }

        Node *cpus_node = _config.get_node_by_path(CPUS_PATH);
        if (cpus_node == nullptr || !node_status_enabled(*cpus_node)) {
            loggers::DEVICE::WARN(
                "设备树中缺少 /cpus 节点或其状态不可用, 无法获取 CPU 信息");
            return;
        }

        auto freq_prop_it = cpus_node->properties.find(TIMEBASE_FREQ_PROP);
        if (freq_prop_it == cpus_node->properties.end()) {
            loggers::DEVICE::WARN(
                "节点 /cpus 缺少 timebase-frequency 属性, CPU 频率保留为 0");
        } else {
            auto timebase_freq = freq_prop_it->second->as_integral();
            if (timebase_freq == 0) {
                loggers::DEVICE::ERROR(
                    "节点 /cpus 的 timebase-frequency 为 0, 无法创建 "
                    "ClockSource");
            } else {
                cpus.freq          = units::frequency::from_hz(timebase_freq);
                cpus._clock_source = new device::CSRTimeClockSource(cpus.freq);
                loggers::DEVICE::INFO(
                    "已创建 CSRTimeClockSource, freq=%lluHz",
                    static_cast<unsigned long long>(cpus.freq.to_hz()));
            }
        }

        std::vector<ParsedCpu> parsed_cpus;
        parsed_cpus.reserve(cpus_node->children.size());
        std::unordered_map<fdt::phandle_t, device::cpuid_t> cpu_phandle_map;
        LocalInterruptTargetMap local_intc_map;

        // 扫描并解析每个 CPU 节点.
        for (const auto *child : sorted_children(*cpus_node)) {
            auto parsed_res = parse_cpu_node(*child);
            if (!parsed_res.has_value()) {
                if (parsed_res.error() == ErrCode::BUSY ||
                    parsed_res.error() == ErrCode::INVALID_PARAM)
                {
                    continue;
                }
                loggers::DEVICE::ERROR("解析 CPU 节点 %s 失败: %s",
                                       child->name.c_str(),
                                       to_cstring(parsed_res.error()));
                return;
            }

            const auto &parsed = parsed_res.value();
            if (cpu_phandle_map.contains(parsed.cpu_phandle)) {
                loggers::DEVICE::ERROR("CPU phandle=%u 被多个 CPU 重复使用",
                                       parsed.cpu_phandle);
                return;
            }
            if (local_intc_map.contains(parsed.local_intc_phandle)) {
                loggers::DEVICE::ERROR(
                    "本地中断 phandle=%u 被多个 CPU 重复使用",
                    parsed.local_intc_phandle);
                return;
            }
            cpu_phandle_map[parsed.cpu_phandle]       = parsed.id;
            local_intc_map[parsed.local_intc_phandle] = parsed.id;
            parsed_cpus.push_back(parsed);
        }

        if (parsed_cpus.empty()) {
            loggers::DEVICE::WARN("未在 /cpus 下解析到可用 CPU");
            return;
        }

        std::ranges::sort(parsed_cpus,
                          [](const ParsedCpu &lhs, const ParsedCpu &rhs) {
                              return lhs.id < rhs.id;
                          });

        // 先为每个 CPU 注册根中断域, 供后续 CLINT 绑定本地中断线使用.
        for (const auto &parsed : parsed_cpus) {
            Node *intc_node = _config.get_node_by_phandle(parsed.cpu_intc_phandle);
            if (intc_node == nullptr) {
                loggers::DEVICE::ERROR("CPU %u 的本地中断节点不存在: phandle=%u",
                                       parsed.id, parsed.cpu_intc_phandle);
                return;
            }

            auto root_node = util::owner<device::DeviceNode *>(
                new FDTDeviceNode(*this, _config, *intc_node));
            auto root_chip_res = device::RiscVIntC::create(
                std::move(root_node), "riscv,cpu-intc",
                static_cast<device::intc_t>(parsed.cpu_intc_phandle),
                parsed.id);
            if (!root_chip_res.has_value()) {
                loggers::DEVICE::ERROR("创建 RiscVIntC 失败: cpu=%u err=%s",
                                       parsed.id,
                                       to_cstring(root_chip_res.error()));
                return;
            }

            auto *root_domain = new device::LinearIrqDomain<CLINT_MAX_HW_IRQ>(
                static_cast<device::domain_t>(parsed.cpu_intc_phandle),
                "riscv,cpu-intc",
                util::owner<device::IrqChip *>(root_chip_res.value().get()));
            if (root_domain == nullptr) {
                loggers::DEVICE::ERROR("分配 RiscVIntC 根域失败: cpu=%u",
                                       parsed.id);
                return;
            }

            auto register_root_res = model.interrupt().register_domain(
                util::owner<device::IrqDomain *>(root_domain));
            if (!register_root_res.has_value()) {
                loggers::DEVICE::ERROR("注册 RiscVIntC 根域失败: cpu=%u err=%s",
                                       parsed.id,
                                       to_cstring(register_root_res.error()));
                return;
            }

            auto irq_domain_res =
                register_irq_domain(parsed.cpu_intc_phandle, *root_domain);
            if (!irq_domain_res.has_value()) {
                loggers::DEVICE::ERROR(
                    "登记 CPU 根中断域失败: cpu=%u phandle=%u err=%s",
                    parsed.id, parsed.cpu_intc_phandle,
                    to_cstring(irq_domain_res.error()));
                return;
            }
        }

        // 构造 CPU 对象列表.
        std::vector<device::cpuid_t> cpu_ids;
        cpu_ids.reserve(parsed_cpus.size());
        for (const auto &parsed : parsed_cpus) {
            auto cpu_res = device::RiscV64Cpu::Builder()
                               .id(parsed.id)
                               .model(parsed.model)
                               .frequency(cpus.freq)
                               .isa_string(parsed.isa_string)
                               .mmu_type(parsed.mmu_type)
                               .local_intc(static_cast<device::intc_t>(
                                   parsed.cpu_intc_phandle))
                               .build();
            if (!cpu_res.has_value()) {
                loggers::DEVICE::ERROR("构建 CPU %u 失败: %s", parsed.id,
                                       to_cstring(cpu_res.error()));
                return;
            }
            cpu_ids.push_back(parsed.id);
            cpus.cpus.emplace_back(cpu_res.value().get());
        }

        auto cpu_map_it = cpus_node->children.find(CPU_MAP_NODE);
        Result<device::CpuTopology> topology_res =
            cpu_map_it != cpus_node->children.end()
                ? build_cpu_map_topology(*cpu_map_it->second, cpu_phandle_map,
                                         cpu_ids)
                : build_default_topology(cpu_ids);

        if (!topology_res.has_value()) {
            loggers::DEVICE::WARN("构建 cpu-map 拓扑失败: %s, 降级为默认拓扑",
                                  to_cstring(topology_res.error()));
            topology_res = build_default_topology(cpu_ids);
        }
        if (!topology_res.has_value()) {
            loggers::DEVICE::ERROR("构建默认 CPU 拓扑失败: %s",
                                   to_cstring(topology_res.error()));
            return;
        }

        cpus.topology = std::move(topology_res.value());
        loggers::DEVICE::INFO(
            "CPU 信息更新完成: freq=%lluHz count=%u topo_cpus=%u",
            static_cast<unsigned long long>(cpus.freq.to_hz()),
            static_cast<unsigned>(cpus.cpus.size()),
            static_cast<unsigned>(cpus.topology.logical_cpus().size()));
    }

    void FDTProvider::register_clint(device::DeviceModel &model) const {
        auto &cpus   = model.cpus();
        if (!_config.root) {
            return;
        }

        Node *cpus_node = _config.get_node_by_path(CPUS_PATH);
        if (cpus_node == nullptr || !node_status_enabled(*cpus_node)) {
            return;
        }

        LocalInterruptTargetMap local_intc_map;
        for (const auto *child : sorted_children(*cpus_node)) {
            auto parsed_res = parse_cpu_node(*child);
            if (!parsed_res.has_value()) {
                if (parsed_res.error() == ErrCode::BUSY ||
                    parsed_res.error() == ErrCode::INVALID_PARAM)
                {
                    continue;
                }
                return;
            }
            const auto &parsed                        = parsed_res.value();
            local_intc_map[parsed.local_intc_phandle] = parsed.id;
        }

        // 扫描并注册 CLINT.
        ClintBackendDescriptor clint_desc;
        find_clint_recursive(*_config.root, clint_desc);

        if (clint_desc.found) {
            if (cpus._clock_source == nullptr) {
                loggers::DEVICE::ERROR(
                    "检测到 CLINT 节点 %s, 但缺少全局 ClockSource, 跳过 CLINT "
                    "注册",
                    clint_desc.name.c_str());
            } else if (clint_desc.node == nullptr) {
                loggers::DEVICE::ERROR("CLINT 节点 %s 缺少节点引用",
                                       clint_desc.name.c_str());
            } else {
                auto refs_res = parse_interrupts_extended(*clint_desc.node);
                if (!refs_res.has_value()) {
                    loggers::DEVICE::ERROR(
                        "解析 CLINT 节点 %s 的 interrupts-extended 失败: %s",
                        clint_desc.name.c_str(), to_cstring(refs_res.error()));
                    return;
                }

                clint_desc.target_harts = target_harts_from_interrupt_refs(
                    clint_desc.name.c_str(), refs_res.value(), local_intc_map);
                if (clint_desc.target_harts.empty()) {
                    loggers::DEVICE::ERROR(
                        "CLINT 节点 %s 未解析到任何目标 hart, 跳过 CLINT 注册",
                        clint_desc.name.c_str());
                    return;
                }

                auto clint_node = util::owner<device::DeviceNode *>(
                    new FDTDeviceNode(*this, _config, *clint_desc.node));
                auto chip_res = device::Clint::create(
                    std::move(clint_node),
                    clint_desc.name.empty() ? "clint" : clint_desc.name,
                    clint_desc.identifier, clint_desc.target_harts.front(),
                    clint_desc.target_harts);
                if (!chip_res.has_value()) {
                    loggers::DEVICE::ERROR("创建 CLINT 后端失败: %s",
                                           to_cstring(chip_res.error()));
                    return;
                }

                auto *domain = new device::LinearIrqDomain<CLINT_MAX_HW_IRQ>(
                    static_cast<device::domain_t>(clint_desc.identifier),
                    clint_desc.name.empty() ? "clint-domain" : clint_desc.name,
                    util::owner<device::IrqChip *>(chip_res.value().get()));
                if (domain == nullptr) {
                    loggers::DEVICE::ERROR("分配 CLINT 中断域失败");
                    return;
                }

                auto register_res = model.interrupt().register_domain(
                    util::owner<device::IrqDomain *>(domain));
                if (!register_res.has_value()) {
                    loggers::DEVICE::ERROR("注册 CLINT 域失败: %s",
                                           to_cstring(register_res.error()));
                    return;
                }

                if (clint_desc.node->phandle != 0) {
                    auto irq_domain_res =
                        register_irq_domain(clint_desc.node->phandle, *domain);
                    if (!irq_domain_res.has_value()) {
                        loggers::DEVICE::ERROR(
                            "登记 CLINT 中断域失败: phandle=%u err=%s",
                            clint_desc.node->phandle,
                            to_cstring(irq_domain_res.error()));
                        return;
                    }
                }

                loggers::DEVICE::INFO(
                    "已注册 CLINT 后端与中断域: %s (domain=%u, harts=%u)",
                    clint_desc.name.c_str(), clint_desc.identifier,
                    static_cast<unsigned>(clint_desc.target_harts.size()));
                // TODO: 通过某种方式让设备后端指定 Alarm
                // 目前先这么做着
                model.set_clock_virq(chip_res.value()->clock_virq());
            }
        } else {
            loggers::DEVICE::WARN(
                "设备树中未找到 riscv,clint0, 仅保留 CPU 本地中断端点信息");
        }
    }

    void FDTProvider::register_plic(device::DeviceModel &model) const {
        auto &irqman = model.interrupt();
        if (!_config.root) {
            return;
        }

        PlicBackendDescriptor plic_desc;
        find_plic_recursive(*_config.root, plic_desc);
        if (!plic_desc.found) {
            loggers::DEVICE::DEBUG("设备树中未找到 riscv,plic0");
            return;
        }
        if (plic_desc.node == nullptr) {
            loggers::DEVICE::ERROR("PLIC 节点 %s 缺少节点引用",
                                   plic_desc.name.c_str());
            return;
        }

        auto refs_res = parse_interrupts_extended(*plic_desc.node);
        if (!refs_res.has_value()) {
            loggers::DEVICE::ERROR(
                "解析 PLIC 节点 %s 的 interrupts-extended 失败: %s",
                plic_desc.name.c_str(), to_cstring(refs_res.error()));
            return;
        }

        const auto &refs = refs_res.value();
        for (size_t index = 0; index < refs.size(); ++index)
        {
            const auto &[phandle, hwirq] = refs[index];
            if (hwirq != device::RiscVIntC::EXTERNAL_LOCAL_IRQ) {
                loggers::DEVICE::DEBUG(
                    "跳过非 external local intc 连接: plic=%s phandle=%u hwirq=%u",
                    plic_desc.name.c_str(), phandle,
                    static_cast<unsigned>(hwirq));
                continue;
            }

            Node *intc_node = _config.get_node_by_phandle(phandle);
            if (intc_node == nullptr || intc_node->parent == nullptr) {
                loggers::DEVICE::WARN("PLIC 节点 %s 的 context phandle=%u 无法定位 CPU",
                                      plic_desc.name.c_str(), phandle);
                continue;
            }

            auto parsed_res = parse_cpu_node(*intc_node->parent);
            if (!parsed_res.has_value()) {
                loggers::DEVICE::WARN(
                    "PLIC 节点 %s 的 context phandle=%u 所属 CPU 解析失败: %s",
                    plic_desc.name.c_str(), phandle,
                    to_cstring(parsed_res.error()));
                continue;
            }

            auto parent_domain_res = resolve_irq_domain(phandle, irqman);
            if (!parent_domain_res.has_value()) {
                loggers::DEVICE::WARN(
                    "PLIC 节点 %s 的 context phandle=%u 缺少已注册中断域: %s",
                    plic_desc.name.c_str(), phandle,
                    to_cstring(parent_domain_res.error()));
                continue;
            }
            auto virq_res = irqman.allocate_virq(parent_domain_res.value().get().id(),
                                                 hwirq);
            if (!virq_res.has_value()) {
                loggers::DEVICE::WARN(
                    "PLIC 节点 %s 的 external_virq 分配失败: phandle=%u hwirq=%u err=%s",
                    plic_desc.name.c_str(), phandle,
                    static_cast<unsigned>(hwirq),
                    to_cstring(virq_res.error()));
                continue;
            }

            plic_desc.contexts.push_back(device::PlicContext{
                .hart_id       = parsed_res.value().id,
                .parent_intc   = static_cast<device::intc_t>(phandle),
                .external_virq = virq_res.value(),
                .context_index = index,
            });
        }

        if (plic_desc.contexts.empty()) {
            loggers::DEVICE::ERROR(
                "PLIC 节点 %s 未解析到任何 supervisor external context",
                plic_desc.name.c_str());
            return;
        }

        auto plic_node = util::owner<device::DeviceNode *>(
            new FDTDeviceNode(*this, _config, *plic_desc.node));
        auto chip_res = device::Plic::create(
            std::move(plic_node),
            plic_desc.name.empty() ? "plic" : plic_desc.name,
            plic_desc.identifier, plic_desc.source_count, plic_desc.contexts);
        if (!chip_res.has_value()) {
            loggers::DEVICE::ERROR("创建 PLIC 后端失败: %s",
                                   to_cstring(chip_res.error()));
            return;
        }

        auto *domain = new device::LinearIrqDomain<MAX_PLIC_IRQS>(
            static_cast<device::domain_t>(plic_desc.identifier),
            plic_desc.name.empty() ? "plic-domain" : plic_desc.name,
            util::owner<device::IrqChip *>(chip_res.value().get()));
        if (domain == nullptr) {
            loggers::DEVICE::ERROR("分配 PLIC 中断域失败");
            return;
        }

        auto register_res = irqman.register_domain(
            util::owner<device::IrqDomain *>(domain));
        if (!register_res.has_value()) {
            loggers::DEVICE::ERROR("注册 PLIC 域失败: %s",
                                   to_cstring(register_res.error()));
            return;
        }

        if (plic_desc.node->phandle != 0) {
            auto irq_domain_res =
                register_irq_domain(plic_desc.node->phandle, *domain);
            if (!irq_domain_res.has_value()) {
                loggers::DEVICE::ERROR(
                    "登记 PLIC 中断域失败: phandle=%u err=%s",
                    plic_desc.node->phandle,
                    to_cstring(irq_domain_res.error()));
                return;
            }
        }

        for (const auto &context : plic_desc.contexts) {
            auto handler_res = irqman.register_handler(
                context.external_virq,
                this_call(new PlicDispatchAction(*chip_res.value(), domain->id()),
                          &PlicDispatchAction::handle));
            if (!handler_res.has_value() &&
                handler_res.error() != ErrCode::KEY_DUPLICATED)
            {
                loggers::DEVICE::ERROR(
                    "注册 PLIC external_virq handler 失败: virq=%llu err=%s",
                    static_cast<unsigned long long>(context.external_virq),
                    to_cstring(handler_res.error()));
                return;
            }
        }

        loggers::DEVICE::INFO(
            "已注册 PLIC 后端与中断域: %s (domain=%u, contexts=%u, sources=%u)",
            plic_desc.name.c_str(), plic_desc.identifier,
            static_cast<unsigned>(plic_desc.contexts.size()),
            static_cast<unsigned>(plic_desc.source_count));
    }

    void FDTProvider::register_clock_virq(device::DeviceModel &model) const {
        loggers::DEVICE::DEBUG(
            "FDTProvider clock_virq=%llu",
            static_cast<unsigned long long>(model.clock_virq()));
    }

    void FDTProvider::register_device(device::DeviceModel &model) const {
        register_memory_regions(model);
        register_cpus(model);
        register_clint(model);
        register_plic(model);
        register_clock_virq(model);
    }
}  // namespace fdt
