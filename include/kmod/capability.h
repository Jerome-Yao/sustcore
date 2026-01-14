/**
 * @file cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力管理接口
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/capability.h>
/**
 * @brief 获取能力类型
 *
 * @param cap 能力
 * @return int 能力类型
 */
int get_cap_type(CapIdx cap);

/**
 * @brief 检查能力是否合法
 *
 * @param cap 能力
 * @return true 合法
 * @return false 不合法
 */
bool valid_cap(CapIdx cap);

/**
 * @brief 为自己创建空能力
 *
 * @param provider 能力提供者能力
 *
 * @return CapIdx 空能力
 */
CapIdx make_null_cap(CapIdx provider);

/**
 * @brief 为自己创建能力
 *
 * 该函数将为进程本身创建一个新的能力，能力的数据指针可以指向与该能力相关的数据结构。
 *
 * @param provider 能力提供者能力
 * @param data_ptr 能力数据指针
 *
 * @return CapIdx 能力
 */
CapIdx make_cap(CapIdx provider, void *data_ptr);

/**
 * @brief 将能力派生给指定进程
 *
 * @param parent 父能力
 * @param pid 目标进程能力, 该能力指向目标进程(被伪装成pid)
 * @return CapIdx 派生后的能力
 */
CapIdx derive_cap(CapIdx parent, CapIdx pid);

/**
 * @brief 使能力失效
 *
 * 该函数将使指定的能力失效，无法再被使用。
 * 同时由该能力派生出的子能力也将一并失效。
 *
 * @param cap 能力
 */
void unvalid_cap(CapIdx cap);