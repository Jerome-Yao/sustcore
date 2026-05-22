/**
 * @file config.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备配置
 * @version alpha-1.0.0
 * @date 2026-05-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>
#include <sustcore/addr.h>

#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct RawIrqDesc {
    // 中断控制器 phandle
    b32 phandle;
    std::vector<b32> hw_irqs;
};

struct RawProperty {
    std::vector<byte> raw;
};

struct RawCompatibleConfig {
    std::string node_name;
    std::vector<std::string> compatibles;
    std::unordered_map<std::string, RawProperty> properties;
};

struct RawRegionalConfig : public RawCompatibleConfig {
    std::vector<VirArea> regions;
};

struct RawDeviceConfig : public RawRegionalConfig {
    b32 default_phandle;
    std::vector<RawIrqDesc> interrupts;
};

struct RawConfiguration {
    std::vector<RawCompatibleConfig> compatibles;
    std::vector<RawRegionalConfig> regionals;
    std::vector<RawDeviceConfig> devices;
};

inline bool has_compatible(const RawDeviceConfig& config,
                           const std::string& compatible) {
    return std::ranges::find(config.compatibles, compatible) !=
           config.compatibles.end();
}

void parse_device_tree(const void* dtb, RawConfiguration& config);
