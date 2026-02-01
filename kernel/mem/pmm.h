/**
 * @file pmm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理内存管理器
 * @version alpha-1.0.0
 * @date 2026-01-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <mem/gfp.h>
#include <sus/types.h>

class PMM {
public:
    struct page {
        int ppn;
        int refcnt;
        int mapcnt;
    };
private:
    static page *__base_address;
    static size_t __arraysz;
    static umb_t __lower_ppn, __upper_ppn;
public:
    static void init(void *lowerbound, void *upperbound);
    static constexpr umb_t phys2ppn(umb_t paddr) {
        return paddr / PAGESIZE;
    }
    static page *__get_page(umb_t ppn);
    inline static page *get_page(void *paddr) {
        return __get_page(phys2ppn((umb_t)paddr));
    }
    inline static void ref_page(page *pg)
    {
        pg->refcnt ++;
        if (pg->refcnt > 1) {
            // Open COW Mechanism
        }
    }
    inline static bool unref_page(page *pg)
    {
        // assert(pg->refcnt > 0);
        pg->refcnt --;
        if (pg->refcnt == 0) {
            return true;
        }
        if (pg->refcnt == 1) {
            // Close COW Mechanism
        }
        return false;
    }
    inline static bool refering(page *pg)
    {
        return pg->refcnt != 0;
    }
    static void reset_page(page *page);
};