/**
 * @file task.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 进程相关结构
 * @version alpha-1.0.0
 * @date 2026-04-14
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <string_view>
#include <cap/cholder.h>
#include <mem/vma.h>

/**
 * @brief Load Parameter
 *
 * 用于标记加载参数, 需要注意的是, 诸如Task Memory, Image File等资源类数据存放在TaskSpec中
 *
 */
struct LoadPrm
{
    // 源路径
    std::string_view src_path;
};

/**
 * @brief Task Specification
 *
 * Task Manager从中构造出task对象
 * 其由Task Manager构造, 并由loader填充
 * 其包含了 Task 所持有的一些资源
 * 所有的资源均由Task Manager构造
 *
 */
struct TaskSpec
{
    // 进程内存管理
    TM *tm;
    // 进程Capability Holder
    CHolder *holder;
};