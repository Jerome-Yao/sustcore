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
#include <libfdt.h>
#include <logger.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *PHANDLE_PROP       = "phandle";
    constexpr const char *LINUX_PHANDLE_PROP = "linux,phandle";
    constexpr const char *REG_PROP           = "reg";
    constexpr const char *ADDRESS_CELLS_PROP = "#address-cells";
    constexpr const char *SIZE_CELLS_PROP    = "#size-cells";
    constexpr const char *NO_MAP_PROP        = "no-map";
    constexpr const char *STATUS_PROP        = "status";
    constexpr const char *DEVICE_TYPE_PROP   = "device_type";
    constexpr const char *TIMEBASE_FREQ_PROP = "timebase-frequency";

    constexpr const char *OKAY_STATUS        = "okay";
    constexpr const char *MEMORY_DEVICE_TYPE = "memory";

    constexpr const char *RESERVED_MEMORY_PATH = "/reserved-memory";
    constexpr const char *CPUS_PATH            = "/cpus";

    [[nodiscard]]
    bool node_status_enabled(const fdt::Node &node) {
        auto it = node.properties.find(STATUS_PROP);
        if (it == node.properties.end()) {
            return true;
        }

        std::string status = it->second->as_string();
        return status == fdt::OKAY_STATUS || status == fdt::OK_STATUS;
    }

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

    [[nodiscard]]
    bool has_property(const fdt::Node &node, const char *prop_name) {
        return node.properties.contains(prop_name);
    }

    [[nodiscard]]
    bool is_string_prop_equal(const fdt::Node &node, const char *prop_name,
                              const char *expected) {
        auto it = node.properties.find(prop_name);
        if (it == node.properties.end()) {
            return false;
        }
        return it->second->as_string() == expected;
    }
}  // namespace

namespace fdt {
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

    void FDTProvider::collect_memory_regions(
        std::vector<device::MemRegion> &regions) const {
        if (!_config.root) {
            return;
        }

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
    }

    void FDTProvider::update_cpus(device::CPUS &cpus) const {
        if (!_config.root) {
            loggers::DEVICE::WARN("设备树根节点不存在, 无法获取 CPU 信息");
            return;
        }

        Node *cpus_node = _config.get_node_by_path(CPUS_PATH);
        if (cpus_node == nullptr || !node_status_enabled(*cpus_node)) {
            loggers::DEVICE::WARN("设备树中缺少 /cpus 节点或其状态不可用, 无法获取 CPU 信息");
            return;
        }

        auto freq_prop_it = cpus_node->properties.find(TIMEBASE_FREQ_PROP);
        if (freq_prop_it == cpus_node->properties.end()) {
            loggers::DEVICE::WARN("节点 /cpus 缺少 timebase-frequency 属性, 无法获取 CPU 频率");
            return;
        }

        cpus.freq =
            units::frequency::from_hz(freq_prop_it->second->as_integral());
    }
}  // namespace fdt
