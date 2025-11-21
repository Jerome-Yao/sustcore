/**
 * @file device.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备信息接口
 * @version alpha-1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

typedef void FDTDescriptor;

/**
 * @brief 检测FDT
 * 
 * @param fdt FDT描述符
 * @return int 错误码
 */
FDTDescriptor *device_check_initial(void *dtb_ptr);

/**
 * @brief 获取最近的设备错误码
 * 
 * @return int 错误码
 */
int device_get_errno(void);

/**
 * @brief 打印整个设备树的所有信息
 * 
 * @param fdt 设备树描述符
 */
void print_entire_device_tree(const FDTDescriptor *fdt);

/**
 * @brief 打印设备树的详细信息
 * 
 * 会额外打印头部信息、内存保留区域和节点统计信息
 * 
 * @param fdt 设备树描述符
 */
void print_device_tree_detailed(const FDTDescriptor *fdt);