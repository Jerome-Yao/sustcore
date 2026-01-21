/**
 * @file pfa.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页框分配器实现
 * @version alpha-1.0.0
 * @date 2026-01-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/pfa.h>
#include <kio.h>
#include <sus/types.h>

void *LinearGrowPFA::baseaddr = nullptr;
void *LinearGrowPFA::curaddr  = nullptr;
void *LinearGrowPFA::boundary = nullptr;

void LinearGrowPFA::pre_init(MemRegion *regions, size_t region_count) {
    // 从regions中找到大小最大的可用内存区域，作为线性增长PFA的内存池
    void *baseaddr  = nullptr;
    size_t max_size = 0;
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].status == MemRegion::MemoryStatus::FREE) {
            if (regions[i].size > max_size) {
                max_size = regions[i].size;
                baseaddr = regions[i].ptr;
            }
        }
    }

    // 将 baseaddr 向上对齐, boundary 向下对齐
    LinearGrowPFA::baseaddr = (void *)page_align_up((umb_t)baseaddr);
    LinearGrowPFA::curaddr  = LinearGrowPFA::baseaddr;
    umb_t _boundary           = (umb_t)baseaddr + max_size;
    LinearGrowPFA::boundary = (void *)page_align_down(_boundary);
}

void LinearGrowPFA::post_init() {}

void *LinearGrowPFA::alloc_frame(size_t frame_count) {
    umb_t _boundary = (umb_t)LinearGrowPFA::curaddr + frame_count * 0x1000;
    if (_boundary > (umb_t)LinearGrowPFA::boundary) {
        return nullptr;  // 内存不足
    }
    void *ptr = LinearGrowPFA::curaddr;
    LinearGrowPFA::curaddr = (void *)_boundary;
    return ptr;
}

void LinearGrowPFA::free_frame(void *ptr, size_t frame_count) {
    // 不需要做任何操作
    (void)ptr;
    (void)frame_count;
}