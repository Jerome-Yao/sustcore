/**
 * @file ext_context.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 扩展上下文保存/恢复
 * @version alpha-1.0.0
 * @date 2026-06-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/trait.h>

#include <cstring>

namespace rv64 {
    namespace {
        [[nodiscard]]
        csr_sstatus_t enable_fpu_temporarily() noexcept {
            csr_sstatus_t original = csr_get_sstatus();
            auto enabled           = original;
            enabled.fs             = XSStatus::DIRTY;
            csr_set_sstatus(enabled);
            return original;
        }

        void restore_sstatus(csr_sstatus_t sstatus) noexcept {
            csr_set_sstatus(sstatus);
        }
    }  // namespace

    void init_ext_context(ExtContext &ctx) noexcept {
        memset(&ctx, 0, sizeof(ctx));
    }

    void save_ext_context(ExtContext &ctx) noexcept {
        auto original = enable_fpu_temporarily();

        asm volatile(".option push\n\t"
                     ".option arch, +f\n\t"
                     ".option arch, +d\n\t"
                     "fsd f0,   0(%0)\n\t"
                     "fsd f1,   8(%0)\n\t"
                     "fsd f2,  16(%0)\n\t"
                     "fsd f3,  24(%0)\n\t"
                     "fsd f4,  32(%0)\n\t"
                     "fsd f5,  40(%0)\n\t"
                     "fsd f6,  48(%0)\n\t"
                     "fsd f7,  56(%0)\n\t"
                     "fsd f8,  64(%0)\n\t"
                     "fsd f9,  72(%0)\n\t"
                     "fsd f10, 80(%0)\n\t"
                     "fsd f11, 88(%0)\n\t"
                     "fsd f12, 96(%0)\n\t"
                     "fsd f13, 104(%0)\n\t"
                     "fsd f14, 112(%0)\n\t"
                     "fsd f15, 120(%0)\n\t"
                     "fsd f16, 128(%0)\n\t"
                     "fsd f17, 136(%0)\n\t"
                     "fsd f18, 144(%0)\n\t"
                     "fsd f19, 152(%0)\n\t"
                     "fsd f20, 160(%0)\n\t"
                     "fsd f21, 168(%0)\n\t"
                     "fsd f22, 176(%0)\n\t"
                     "fsd f23, 184(%0)\n\t"
                     "fsd f24, 192(%0)\n\t"
                     "fsd f25, 200(%0)\n\t"
                     "fsd f26, 208(%0)\n\t"
                     "fsd f27, 216(%0)\n\t"
                     "fsd f28, 224(%0)\n\t"
                     "fsd f29, 232(%0)\n\t"
                     "fsd f30, 240(%0)\n\t"
                     "fsd f31, 248(%0)\n\t"
                     ".option pop\n\t"
                     :
                     : "r"(&ctx)
                     : "memory");

        umb_t fcsr = 0;
        asm volatile(".option push\n\t"
                     ".option arch, +f\n\t"
                     "frcsr %0\n\t"
                     ".option pop"
                     : "=r"(fcsr));
        ctx.fcsr = static_cast<b32>(fcsr);

        restore_sstatus(original);
    }

    void restore_ext_context(const ExtContext &ctx) noexcept {
        auto original = enable_fpu_temporarily();

        asm volatile(".option push\n\t"
                     ".option arch, +f\n\t"
                     ".option arch, +d\n\t"
                     "fld f0,   0(%0)\n\t"
                     "fld f1,   8(%0)\n\t"
                     "fld f2,  16(%0)\n\t"
                     "fld f3,  24(%0)\n\t"
                     "fld f4,  32(%0)\n\t"
                     "fld f5,  40(%0)\n\t"
                     "fld f6,  48(%0)\n\t"
                     "fld f7,  56(%0)\n\t"
                     "fld f8,  64(%0)\n\t"
                     "fld f9,  72(%0)\n\t"
                     "fld f10, 80(%0)\n\t"
                     "fld f11, 88(%0)\n\t"
                     "fld f12, 96(%0)\n\t"
                     "fld f13, 104(%0)\n\t"
                     "fld f14, 112(%0)\n\t"
                     "fld f15, 120(%0)\n\t"
                     "fld f16, 128(%0)\n\t"
                     "fld f17, 136(%0)\n\t"
                     "fld f18, 144(%0)\n\t"
                     "fld f19, 152(%0)\n\t"
                     "fld f20, 160(%0)\n\t"
                     "fld f21, 168(%0)\n\t"
                     "fld f22, 176(%0)\n\t"
                     "fld f23, 184(%0)\n\t"
                     "fld f24, 192(%0)\n\t"
                     "fld f25, 200(%0)\n\t"
                     "fld f26, 208(%0)\n\t"
                     "fld f27, 216(%0)\n\t"
                     "fld f28, 224(%0)\n\t"
                     "fld f29, 232(%0)\n\t"
                     "fld f30, 240(%0)\n\t"
                     "fld f31, 248(%0)\n\t"
                     ".option pop\n\t"
                     :
                     : "r"(&ctx)
                     : "memory");

        umb_t fcsr = ctx.fcsr;
        asm volatile(".option push\n\t"
                     ".option arch, +f\n\t"
                     "fscsr %0\n\t"
                     ".option pop"
                     :
                     : "r"(fcsr)
                     : "memory");

        restore_sstatus(original);
    }

    void copy_ext_context(ExtContext &dst, const ExtContext &src) noexcept {
        memcpy(&dst, &src, sizeof(dst));
    }
}  // namespace rv64
