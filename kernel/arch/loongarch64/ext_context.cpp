/**
 * @file ext_context.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 扩展上下文保存/恢复
 * @version alpha-1.0.0
 * @date 2026-06-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/csr.h>
#include <arch/loongarch64/trait.h>

#include <cstring>

namespace la64 {
    namespace {
        [[nodiscard]]
        csr_euen_t enable_ext_temporarily() noexcept {
            csr_euen_t original = csr_get_euen();
            auto enabled        = original;
            enabled.fpe         = 1;
            enabled.sxe         = 1;
            csr_set_euen(enabled);
            return original;
        }

        void restore_euen(csr_euen_t euen) noexcept {
            csr_set_euen(euen);
        }
    }  // namespace

    void init_ext_context(ExtContext &ctx) noexcept {
        memset(&ctx, 0, sizeof(ctx));
    }

    void save_ext_context(ExtContext &ctx) noexcept {
        auto original = enable_ext_temporarily();

        asm volatile("vst $vr0,  %0, 0\n\t"
                     "vst $vr1,  %0, 16\n\t"
                     "vst $vr2,  %0, 32\n\t"
                     "vst $vr3,  %0, 48\n\t"
                     "vst $vr4,  %0, 64\n\t"
                     "vst $vr5,  %0, 80\n\t"
                     "vst $vr6,  %0, 96\n\t"
                     "vst $vr7,  %0, 112\n\t"
                     "vst $vr8,  %0, 128\n\t"
                     "vst $vr9,  %0, 144\n\t"
                     "vst $vr10, %0, 160\n\t"
                     "vst $vr11, %0, 176\n\t"
                     "vst $vr12, %0, 192\n\t"
                     "vst $vr13, %0, 208\n\t"
                     "vst $vr14, %0, 224\n\t"
                     "vst $vr15, %0, 240\n\t"
                     "vst $vr16, %0, 256\n\t"
                     "vst $vr17, %0, 272\n\t"
                     "vst $vr18, %0, 288\n\t"
                     "vst $vr19, %0, 304\n\t"
                     "vst $vr20, %0, 320\n\t"
                     "vst $vr21, %0, 336\n\t"
                     "vst $vr22, %0, 352\n\t"
                     "vst $vr23, %0, 368\n\t"
                     "vst $vr24, %0, 384\n\t"
                     "vst $vr25, %0, 400\n\t"
                     "vst $vr26, %0, 416\n\t"
                     "vst $vr27, %0, 432\n\t"
                     "vst $vr28, %0, 448\n\t"
                     "vst $vr29, %0, 464\n\t"
                     "vst $vr30, %0, 480\n\t"
                     "vst $vr31, %0, 496\n\t"
                     :
                     : "r"(&ctx)
                     : "memory");

        umb_t fcc0 = 0;
        umb_t fcc1 = 0;
        umb_t fcc2 = 0;
        umb_t fcc3 = 0;
        umb_t fcc4 = 0;
        umb_t fcc5 = 0;
        umb_t fcc6 = 0;
        umb_t fcc7 = 0;
        asm volatile("movcf2gr %0, $fcc0\n\t"
                     "movcf2gr %1, $fcc1\n\t"
                     "movcf2gr %2, $fcc2\n\t"
                     "movcf2gr %3, $fcc3\n\t"
                     "movcf2gr %4, $fcc4\n\t"
                     "movcf2gr %5, $fcc5\n\t"
                     "movcf2gr %6, $fcc6\n\t"
                     "movcf2gr %7, $fcc7\n\t"
                     : "=r"(fcc0), "=r"(fcc1), "=r"(fcc2), "=r"(fcc3),
                       "=r"(fcc4), "=r"(fcc5), "=r"(fcc6), "=r"(fcc7));
        ctx.fcc = (fcc0 & 0xffULL) | ((fcc1 & 0xffULL) << 8) |
                  ((fcc2 & 0xffULL) << 16) | ((fcc3 & 0xffULL) << 24) |
                  ((fcc4 & 0xffULL) << 32) | ((fcc5 & 0xffULL) << 40) |
                  ((fcc6 & 0xffULL) << 48) | ((fcc7 & 0xffULL) << 56);

        umb_t fcsr = 0;
        asm volatile("movfcsr2gr %0, $fcsr0" : "=r"(fcsr));
        ctx.fcsr = static_cast<u32>(fcsr);

        restore_euen(original);
    }

    void restore_ext_context(const ExtContext &ctx) noexcept {
        auto original = enable_ext_temporarily();

        asm volatile("vld $vr0,  %0, 0\n\t"
                     "vld $vr1,  %0, 16\n\t"
                     "vld $vr2,  %0, 32\n\t"
                     "vld $vr3,  %0, 48\n\t"
                     "vld $vr4,  %0, 64\n\t"
                     "vld $vr5,  %0, 80\n\t"
                     "vld $vr6,  %0, 96\n\t"
                     "vld $vr7,  %0, 112\n\t"
                     "vld $vr8,  %0, 128\n\t"
                     "vld $vr9,  %0, 144\n\t"
                     "vld $vr10, %0, 160\n\t"
                     "vld $vr11, %0, 176\n\t"
                     "vld $vr12, %0, 192\n\t"
                     "vld $vr13, %0, 208\n\t"
                     "vld $vr14, %0, 224\n\t"
                     "vld $vr15, %0, 240\n\t"
                     "vld $vr16, %0, 256\n\t"
                     "vld $vr17, %0, 272\n\t"
                     "vld $vr18, %0, 288\n\t"
                     "vld $vr19, %0, 304\n\t"
                     "vld $vr20, %0, 320\n\t"
                     "vld $vr21, %0, 336\n\t"
                     "vld $vr22, %0, 352\n\t"
                     "vld $vr23, %0, 368\n\t"
                     "vld $vr24, %0, 384\n\t"
                     "vld $vr25, %0, 400\n\t"
                     "vld $vr26, %0, 416\n\t"
                     "vld $vr27, %0, 432\n\t"
                     "vld $vr28, %0, 448\n\t"
                     "vld $vr29, %0, 464\n\t"
                     "vld $vr30, %0, 480\n\t"
                     "vld $vr31, %0, 496\n\t"
                     :
                     : "r"(&ctx)
                     : "memory");

        umb_t fcc = ctx.fcc;
        umb_t fcc0 = (fcc >> 0) & 0xffULL;
        umb_t fcc1 = (fcc >> 8) & 0xffULL;
        umb_t fcc2 = (fcc >> 16) & 0xffULL;
        umb_t fcc3 = (fcc >> 24) & 0xffULL;
        umb_t fcc4 = (fcc >> 32) & 0xffULL;
        umb_t fcc5 = (fcc >> 40) & 0xffULL;
        umb_t fcc6 = (fcc >> 48) & 0xffULL;
        umb_t fcc7 = (fcc >> 56) & 0xffULL;
        asm volatile("movgr2cf $fcc0, %0\n\t"
                     "movgr2cf $fcc1, %1\n\t"
                     "movgr2cf $fcc2, %2\n\t"
                     "movgr2cf $fcc3, %3\n\t"
                     "movgr2cf $fcc4, %4\n\t"
                     "movgr2cf $fcc5, %5\n\t"
                     "movgr2cf $fcc6, %6\n\t"
                     "movgr2cf $fcc7, %7\n\t"
                     :
                     : "r"(fcc0), "r"(fcc1), "r"(fcc2), "r"(fcc3), "r"(fcc4),
                       "r"(fcc5), "r"(fcc6), "r"(fcc7)
                     : "memory");

        umb_t fcsr = ctx.fcsr;
        asm volatile("movgr2fcsr $fcsr0, %0" : : "r"(fcsr) : "memory");

        restore_euen(original);
    }

    void copy_ext_context(ExtContext &dst, const ExtContext &src) noexcept {
        memcpy(&dst, &src, sizeof(dst));
    }
}  // namespace la64
