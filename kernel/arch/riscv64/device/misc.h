/**
 * @file misc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 杂项
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/units.h>
#include <cstddef>

/**
 * @brief 获得时钟频率
 *
 * @return units::frequency 时钟频率
 */
units::frequency get_clock_freq(void);
