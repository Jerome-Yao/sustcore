/**
 * @file kaddr.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核地址空间
 * @version alpha-1.0.0
 * @date 2026-01-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/kaddr.h>
#include <symbols.h>

namespace ker_paddr {
    struct Segment {
        void *start, *end;
        void *vstart, *vend;

        Segment() = default;

        Segment(void *s, void *e)
            : start(s), end(e), vstart(PA2KA(s)), vend(PA2KA(e)) {}

        Segment(void *s, void *e, void *vs, void *ve)
            : start(s), end(e), vstart(vs), vend(ve) {}

        size_t size() const {
            return (char *)end - (char *)start;
        }
    };
    Segment kernel;
    Segment text;
    Segment ivt;
    Segment rodata;
    Segment data;
    Segment bss;
    Segment misc;

    Segment kphy_space;

    void init(void *upper_bound) {
        ker_paddr::kernel = Segment(&skernel, &ekernel);
        ker_paddr::text   = Segment(&s_text, &e_text);
        ker_paddr::ivt    = Segment(&s_ivt, &e_ivt);
        ker_paddr::rodata = Segment(&s_rodata, &e_rodata);
        ker_paddr::data   = Segment(&s_data, &e_data);
        ker_paddr::bss    = Segment(&s_bss, &e_bss);
        ker_paddr::misc   = Segment(&s_misc, &ekernel);

        ker_paddr::kphy_space =
            Segment((void *)0, upper_bound, (void *)(0 + KPHY_VA_OFFSET),
                    (void *)((umb_t)(upper_bound) + KPHY_VA_OFFSET));
    }

    void map_seg(PageMan &man, const Segment &seg, PageMan::RWX rwx, bool u,
                 bool g) {
        man.map_range<true>(seg.vstart, seg.start, seg.size(), rwx, u, g);
    }

    void mapping_kernel_areas(PageMan &man) {
        // TODO: 专门维持一个内核页表, 其它页表可以直接复用该内核页表, 不需要二次构造
        map_seg(man, text, PageMan::rwx(true, false, true), false, true);
        map_seg(man, ivt, PageMan::rwx(true, false, true), false, true);
        map_seg(man, rodata, PageMan::rwx(true, false, false), false, true);
        map_seg(man, data, PageMan::rwx(true, true, false), false, true);
        map_seg(man, bss, PageMan::rwx(true, true, false), false, true);
        map_seg(man, misc, PageMan::rwx(true, false, false), false, true);
        map_seg(man, kphy_space, PageMan::rwx(true, true, false), false, true);
    }
}  // namespace ker_paddr