/**
 * @file misc.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 杂项
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/device/misc.h>
#include <device/model.h>
#include <logger.h>
#include <sbi/sbi.h>
#include <sus/logger.h>
#include <sus/units.h>

units::frequency get_clock_freq(void) {
    // 读取 /cpus/timebase-frequency 属性
    return device::DeviceModel::inst().cpus().freq;
}
