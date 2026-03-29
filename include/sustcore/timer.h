/**
 * @file timer.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief timer
 * @version alpha-1.0.0
 * @date 2026-03-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/units.h>

struct TimerTickInfo {
    units::tick last_tick;
    units::tick increment;
    units::tick gap_ticks;
};