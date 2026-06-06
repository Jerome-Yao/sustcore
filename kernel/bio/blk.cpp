/**
 * @file blk.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 块设备管理器实现
 * @version alpha-1.0.0
 * @date 2026-06-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <bio/blk.h>

namespace blk {
    BlkManager BlkManager::_INSTANCE {};
    bool BlkManager::_initialized = false;
}  // namespace blk
