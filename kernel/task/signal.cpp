/**
 * @file signal.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Linux ABI signal 第二阶段实现
 * @version alpha-1.0.0
 * @date 2026-06-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#include <object/task.h>
#include <syscall/uaccess.h>
#include <syscall.h.in>
#include <task/task.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace task {
    namespace {
        constexpr size_t LINUX_SIG_DFL = 0;
        constexpr size_t LINUX_SIG_IGN = 1;

        constexpr uint64_t LINUX_SA_SIGINFO   = 0x00000004UL;
        constexpr uint64_t LINUX_SA_RESTORER  = 0x04000000UL;

        constexpr size_t LINUX_SIGKILL = 9;
        constexpr size_t LINUX_SIGUSR1 = 10;
        constexpr size_t LINUX_SIGSEGV = 11;
        constexpr size_t LINUX_SIGUSR2 = 12;
        constexpr size_t LINUX_SIGTERM = 15;
        constexpr size_t LINUX_SIGCHLD = 17;

        constexpr uint64_t LINUX_SI_USER = 0;

        [[nodiscard]]
        bool is_default_ignored_signal(size_t signo) noexcept {
            return signo == LINUX_SIGCHLD;
        }

        [[nodiscard]]
        bool is_default_terminating_signal(size_t signo) noexcept {
            switch (signo) {
                case LINUX_SIGTERM:
                case LINUX_SIGKILL:
                case LINUX_SIGSEGV: return true;
                default:            return false;
            }
        }

        [[nodiscard]]
        int default_exit_code_for_signal(size_t signo) noexcept {
            return 128 + static_cast<int>(signo);
        }

        [[nodiscard]]
        size_t first_signal_index(uint64_t bits) noexcept {
            size_t index = 0;
            while ((bits & 1U) == 0U) {
                bits >>= 1U;
                ++index;
            }
            return index;
        }

        struct linux_sigset_t {
            uint64_t sig[1];
        };

        struct linux_siginfo_t {
            int32_t si_signo;
            int32_t si_errno;
            int32_t si_code;
            int32_t __pad0;
            uint8_t _pad[112];
        };

        struct linux_stack_t {
            uint64_t ss_sp;
            int32_t ss_flags;
            int32_t __pad0;
            uint64_t ss_size;
        };

#if defined(__ARCH_riscv64__)
        constexpr uint64_t LINUX_FRAME_FROM_SYSCALL = 1U;
        constexpr uint64_t LINUX_USER_STATUS_BITS =
            (1ULL << 18) | (1ULL << 13) | (1ULL << 5);
        constexpr size_t LINUX_SYSCALL_INS_LEN = 4U;

        struct linux_riscv64_user_regs {
            uint64_t pc;
            uint64_t ra;
            uint64_t sp;
            uint64_t gp;
            uint64_t tp;
            uint64_t t0;
            uint64_t t1;
            uint64_t t2;
            uint64_t s0;
            uint64_t s1;
            uint64_t a0;
            uint64_t a1;
            uint64_t a2;
            uint64_t a3;
            uint64_t a4;
            uint64_t a5;
            uint64_t a6;
            uint64_t a7;
            uint64_t s2;
            uint64_t s3;
            uint64_t s4;
            uint64_t s5;
            uint64_t s6;
            uint64_t s7;
            uint64_t s8;
            uint64_t s9;
            uint64_t s10;
            uint64_t s11;
            uint64_t t3;
            uint64_t t4;
            uint64_t t5;
            uint64_t t6;
        };

        struct linux_riscv64_ctx_header {
            uint32_t magic;
            uint32_t size;
        };

        struct linux_riscv64_d_ext_state {
            uint64_t f[32];
            uint32_t fcsr;
        };

        struct linux_riscv64_extra_ext_header {
            uint32_t __padding[129] __attribute__((aligned(16)));
            uint32_t reserved;
            linux_riscv64_ctx_header hdr;
        };

        union linux_riscv64_fp_state {
            linux_riscv64_d_ext_state d;
            linux_riscv64_extra_ext_header q;
        };

        struct linux_riscv64_sigcontext {
            linux_riscv64_user_regs sc_regs;
            union {
                linux_riscv64_fp_state sc_fpregs;
                linux_riscv64_extra_ext_header sc_extdesc;
            };
        };

        struct linux_riscv64_ucontext {
            uint64_t uc_flags;
            linux_riscv64_ucontext *uc_link;
            linux_stack_t uc_stack;
            linux_sigset_t uc_sigmask;
            uint8_t __unused[1024 / 8 - sizeof(linux_sigset_t)];
            linux_riscv64_sigcontext uc_mcontext;
        };

        struct linux_riscv64_frame {
            linux_siginfo_t info;
            linux_riscv64_ucontext ucontext;
            uint64_t flags;
            uint64_t syscall_pc;
        };

        static_assert(sizeof(linux_sigset_t) +
                           sizeof(((linux_riscv64_ucontext *)0)->__unused) ==
                       1024 / 8);
        static_assert(offsetof(linux_riscv64_ucontext, uc_mcontext) == 176);
        static_assert(offsetof(linux_riscv64_sigcontext, sc_regs.pc) == 0);

        [[nodiscard]]
        uint64_t user_mask_to_kernel_mask(uint64_t mask) noexcept {
            uint64_t kernel_mask = mask << 1U;
            kernel_mask &= ~(uint64_t(1) << LINUX_SIGKILL);
            return kernel_mask;
        }

        [[nodiscard]]
        uint64_t kernel_mask_to_user_mask(uint64_t mask) noexcept {
            return mask >> 1U;
        }

        void fill_user_siginfo(linux_siginfo_t &info, size_t signo) noexcept {
            memset(&info, 0, sizeof(info));
            info.si_signo = static_cast<int32_t>(signo);
            info.si_errno = 0;
            info.si_code  = static_cast<int32_t>(LINUX_SI_USER);
        }

        void fill_ucontext(linux_riscv64_ucontext &ucontext, const Context &ctx,
                           uint64_t blocked_mask,
                           const ExtContext *ext_ctx) noexcept {
            memset(&ucontext, 0, sizeof(ucontext));
            ucontext.uc_sigmask.sig[0] = kernel_mask_to_user_mask(blocked_mask);

            ucontext.uc_mcontext.sc_regs.pc  = ctx.pc();
            ucontext.uc_mcontext.sc_regs.ra  = ctx.ra();
            ucontext.uc_mcontext.sc_regs.sp  = ctx.sp();
            ucontext.uc_mcontext.sc_regs.gp  = ctx.gp;
            ucontext.uc_mcontext.sc_regs.tp  = ctx.tp;
            ucontext.uc_mcontext.sc_regs.t0  = ctx.t0;
            ucontext.uc_mcontext.sc_regs.t1  = ctx.t1;
            ucontext.uc_mcontext.sc_regs.t2  = ctx.t2;
            ucontext.uc_mcontext.sc_regs.s0  = ctx.s0;
            ucontext.uc_mcontext.sc_regs.s1  = ctx.s1;
            ucontext.uc_mcontext.sc_regs.a0  = ctx.a0;
            ucontext.uc_mcontext.sc_regs.a1  = ctx.a1;
            ucontext.uc_mcontext.sc_regs.a2  = ctx.a2;
            ucontext.uc_mcontext.sc_regs.a3  = ctx.a3;
            ucontext.uc_mcontext.sc_regs.a4  = ctx.a4;
            ucontext.uc_mcontext.sc_regs.a5  = ctx.a5;
            ucontext.uc_mcontext.sc_regs.a6  = ctx.a6;
            ucontext.uc_mcontext.sc_regs.a7  = ctx.a7;
            ucontext.uc_mcontext.sc_regs.s2  = ctx.s2;
            ucontext.uc_mcontext.sc_regs.s3  = ctx.s3;
            ucontext.uc_mcontext.sc_regs.s4  = ctx.s4;
            ucontext.uc_mcontext.sc_regs.s5  = ctx.s5;
            ucontext.uc_mcontext.sc_regs.s6  = ctx.s6;
            ucontext.uc_mcontext.sc_regs.s7  = ctx.s7;
            ucontext.uc_mcontext.sc_regs.s8  = ctx.s8;
            ucontext.uc_mcontext.sc_regs.s9  = ctx.s9;
            ucontext.uc_mcontext.sc_regs.s10 = ctx.s10;
            ucontext.uc_mcontext.sc_regs.s11 = ctx.s11;
            ucontext.uc_mcontext.sc_regs.t3  = ctx.t3;
            ucontext.uc_mcontext.sc_regs.t4  = ctx.t4;
            ucontext.uc_mcontext.sc_regs.t5  = ctx.t5;
            ucontext.uc_mcontext.sc_regs.t6  = ctx.t6;

            memset(&ucontext.uc_mcontext.sc_fpregs, 0,
                   sizeof(ucontext.uc_mcontext.sc_fpregs));
            if (ext_ctx != nullptr) {
                memcpy(ucontext.uc_mcontext.sc_fpregs.d.f, ext_ctx->f,
                       sizeof(ext_ctx->f));
                ucontext.uc_mcontext.sc_fpregs.d.fcsr = ext_ctx->fcsr;
            }
        }

        void restore_context(Context &ctx,
                             const linux_riscv64_ucontext &ucontext) noexcept {
            ctx.ra()  = ucontext.uc_mcontext.sc_regs.ra;
            ctx.sp()  = ucontext.uc_mcontext.sc_regs.sp;
            ctx.gp    = ucontext.uc_mcontext.sc_regs.gp;
            ctx.tp    = ucontext.uc_mcontext.sc_regs.tp;
            ctx.t0    = ucontext.uc_mcontext.sc_regs.t0;
            ctx.t1    = ucontext.uc_mcontext.sc_regs.t1;
            ctx.t2    = ucontext.uc_mcontext.sc_regs.t2;
            ctx.s0    = ucontext.uc_mcontext.sc_regs.s0;
            ctx.s1    = ucontext.uc_mcontext.sc_regs.s1;
            ctx.a0    = ucontext.uc_mcontext.sc_regs.a0;
            ctx.a1    = ucontext.uc_mcontext.sc_regs.a1;
            ctx.a2    = ucontext.uc_mcontext.sc_regs.a2;
            ctx.a3    = ucontext.uc_mcontext.sc_regs.a3;
            ctx.a4    = ucontext.uc_mcontext.sc_regs.a4;
            ctx.a5    = ucontext.uc_mcontext.sc_regs.a5;
            ctx.a6    = ucontext.uc_mcontext.sc_regs.a6;
            ctx.a7    = ucontext.uc_mcontext.sc_regs.a7;
            ctx.s2    = ucontext.uc_mcontext.sc_regs.s2;
            ctx.s3    = ucontext.uc_mcontext.sc_regs.s3;
            ctx.s4    = ucontext.uc_mcontext.sc_regs.s4;
            ctx.s5    = ucontext.uc_mcontext.sc_regs.s5;
            ctx.s6    = ucontext.uc_mcontext.sc_regs.s6;
            ctx.s7    = ucontext.uc_mcontext.sc_regs.s7;
            ctx.s8    = ucontext.uc_mcontext.sc_regs.s8;
            ctx.s9    = ucontext.uc_mcontext.sc_regs.s9;
            ctx.s10   = ucontext.uc_mcontext.sc_regs.s10;
            ctx.s11   = ucontext.uc_mcontext.sc_regs.s11;
            ctx.t3    = ucontext.uc_mcontext.sc_regs.t3;
            ctx.t4    = ucontext.uc_mcontext.sc_regs.t4;
            ctx.t5    = ucontext.uc_mcontext.sc_regs.t5;
            ctx.t6    = ucontext.uc_mcontext.sc_regs.t6;
            ctx.pc()  = ucontext.uc_mcontext.sc_regs.pc;
            ctx.sstatus.value = LINUX_USER_STATUS_BITS;
        }

        void restore_ext(const linux_riscv64_ucontext &ucontext,
                         ExtContext *ext_ctx) noexcept {
            if (ext_ctx == nullptr) {
                return;
            }
            memcpy(ext_ctx->f, ucontext.uc_mcontext.sc_fpregs.d.f,
                   sizeof(ext_ctx->f));
            ext_ctx->fcsr = ucontext.uc_mcontext.sc_fpregs.d.fcsr;
            ext_ctx->reserved = 0;
        }

#elif defined(__ARCH_loongarch64__)
        constexpr uint64_t LINUX_FRAME_FROM_SYSCALL = 1U;
        constexpr uint64_t LINUX_SYSCALL_INS_LEN = 4U;

        struct linux_loongarch64_sigcontext {
            uint64_t sc_pc;
            uint64_t sc_regs[32];
            uint32_t sc_flags;
            uint32_t __pad0;
            uint64_t sc_extcontext[0] __attribute__((aligned(16)));
        };

        struct linux_loongarch64_sctx_info {
            uint32_t magic;
            uint32_t size;
            uint64_t padding;
        };

        struct linux_loongarch64_lsx_context {
            uint64_t regs[2 * 32];
            uint64_t fcc;
            uint32_t fcsr;
            uint32_t __pad0;
        };

        struct linux_loongarch64_ucontext {
            uint64_t uc_flags;
            linux_loongarch64_ucontext *uc_link;
            linux_stack_t uc_stack;
            linux_sigset_t uc_sigmask;
            uint8_t __unused[1024 / 8 - sizeof(linux_sigset_t)];
            linux_loongarch64_sigcontext uc_mcontext;
        };

        struct linux_loongarch64_lsx_extcontext {
            linux_loongarch64_sctx_info header;
            linux_loongarch64_lsx_context context;
        } __attribute__((aligned(16)));

        struct linux_loongarch64_extcontext {
            linux_loongarch64_lsx_extcontext lsx;
            linux_loongarch64_sctx_info end;
        } __attribute__((aligned(16)));

        struct linux_loongarch64_frame {
            linux_siginfo_t info;
            linux_loongarch64_ucontext ucontext;
            linux_loongarch64_extcontext extcontext;
            uint64_t flags;
            uint64_t syscall_pc;
        };

        static_assert(sizeof(linux_sigset_t) +
                           sizeof(((linux_loongarch64_ucontext *)0)->__unused) ==
                       1024 / 8);
        static_assert(offsetof(linux_loongarch64_ucontext, uc_mcontext) == 176);

        [[nodiscard]]
        uint64_t user_mask_to_kernel_mask(uint64_t mask) noexcept {
            uint64_t kernel_mask = mask << 1U;
            kernel_mask &= ~(uint64_t(1) << LINUX_SIGKILL);
            return kernel_mask;
        }

        [[nodiscard]]
        uint64_t kernel_mask_to_user_mask(uint64_t mask) noexcept {
            return mask >> 1U;
        }

        void fill_user_siginfo(linux_siginfo_t &info, size_t signo) noexcept {
            memset(&info, 0, sizeof(info));
            info.si_signo = static_cast<int32_t>(signo);
            info.si_errno = 0;
            info.si_code  = static_cast<int32_t>(LINUX_SI_USER);
        }

        void fill_ucontext(linux_loongarch64_ucontext &ucontext,
                           const Context &ctx, uint64_t blocked_mask,
                           const ExtContext *ext_ctx) noexcept {
            memset(&ucontext, 0, sizeof(ucontext));
            ucontext.uc_sigmask.sig[0] = kernel_mask_to_user_mask(blocked_mask);
            auto &sc = ucontext.uc_mcontext;
            sc.sc_pc      = ctx.pc();
            sc.sc_regs[0] = 0;
            sc.sc_regs[1] = ctx.ra();
            sc.sc_regs[2] = ctx.tp;
            sc.sc_regs[3] = ctx.sp();
            sc.sc_regs[4] = ctx.a0;
            sc.sc_regs[5] = ctx.a1;
            sc.sc_regs[6] = ctx.a2;
            sc.sc_regs[7] = ctx.a3;
            sc.sc_regs[8] = ctx.a4;
            sc.sc_regs[9] = ctx.a5;
            sc.sc_regs[10] = ctx.a6;
            sc.sc_regs[11] = ctx.a7;
            sc.sc_regs[12] = ctx.t0;
            sc.sc_regs[13] = ctx.t1;
            sc.sc_regs[14] = ctx.t2;
            sc.sc_regs[15] = ctx.t3;
            sc.sc_regs[16] = ctx.t4;
            sc.sc_regs[17] = ctx.t5;
            sc.sc_regs[18] = ctx.t6;
            sc.sc_regs[19] = ctx.t7;
            sc.sc_regs[20] = ctx.t8;
            sc.sc_regs[21] = ctx.u0;
            sc.sc_regs[22] = ctx.fp;
            sc.sc_regs[23] = ctx.s0;
            sc.sc_regs[24] = ctx.s1;
            sc.sc_regs[25] = ctx.s2;
            sc.sc_regs[26] = ctx.s3;
            sc.sc_regs[27] = ctx.s4;
            sc.sc_regs[28] = ctx.s5;
            sc.sc_regs[29] = ctx.s6;
            sc.sc_regs[30] = ctx.s7;
            sc.sc_regs[31] = ctx.s8;
            sc.sc_flags = 1U;
            (void)ext_ctx;
        }

        void fill_extcontext(linux_loongarch64_extcontext &extcontext,
                             const ExtContext *ext_ctx) noexcept {
            memset(&extcontext, 0, sizeof(extcontext));
            if (ext_ctx == nullptr) {
                return;
            }
            extcontext.lsx.header.magic = 0x53580001U;
            extcontext.lsx.header.size =
                sizeof(linux_loongarch64_lsx_extcontext);
            memcpy(extcontext.lsx.context.regs, ext_ctx->v,
                   sizeof(ext_ctx->v));
            extcontext.lsx.context.fcc  = ext_ctx->fcc;
            extcontext.lsx.context.fcsr = ext_ctx->fcsr;
        }

        void restore_context(Context &ctx,
                             const linux_loongarch64_ucontext &ucontext) noexcept {
            const auto &sc = ucontext.uc_mcontext;
            ctx.ra() = sc.sc_regs[1];
            ctx.tp   = sc.sc_regs[2];
            ctx.sp() = sc.sc_regs[3];
            ctx.a0   = sc.sc_regs[4];
            ctx.a1   = sc.sc_regs[5];
            ctx.a2   = sc.sc_regs[6];
            ctx.a3   = sc.sc_regs[7];
            ctx.a4   = sc.sc_regs[8];
            ctx.a5   = sc.sc_regs[9];
            ctx.a6   = sc.sc_regs[10];
            ctx.a7   = sc.sc_regs[11];
            ctx.t0   = sc.sc_regs[12];
            ctx.t1   = sc.sc_regs[13];
            ctx.t2   = sc.sc_regs[14];
            ctx.t3   = sc.sc_regs[15];
            ctx.t4   = sc.sc_regs[16];
            ctx.t5   = sc.sc_regs[17];
            ctx.t6   = sc.sc_regs[18];
            ctx.t7   = sc.sc_regs[19];
            ctx.t8   = sc.sc_regs[20];
            ctx.u0   = sc.sc_regs[21];
            ctx.fp   = sc.sc_regs[22];
            ctx.s0   = sc.sc_regs[23];
            ctx.s1   = sc.sc_regs[24];
            ctx.s2   = sc.sc_regs[25];
            ctx.s3   = sc.sc_regs[26];
            ctx.s4   = sc.sc_regs[27];
            ctx.s5   = sc.sc_regs[28];
            ctx.s6   = sc.sc_regs[29];
            ctx.s7   = sc.sc_regs[30];
            ctx.s8   = sc.sc_regs[31];
            ctx.pc() = sc.sc_pc;
            ctx.crmd = {};
            ctx.prmd = {};
            ctx.crmd.plv  = PLV_KERNEL;
            ctx.crmd.ie   = 0;
            ctx.crmd.da   = 0;
            ctx.crmd.pg   = 1;
            ctx.crmd.datf = 0b01;
            ctx.crmd.datm = 0b01;
            ctx.prmd.pplv = PLV_USER;
            ctx.prmd.pie  = 1;
        }

        void restore_ext(const linux_loongarch64_frame &frame,
                         ExtContext *ext_ctx) noexcept {
            if (ext_ctx == nullptr) {
                return;
            }
            memcpy(ext_ctx->v, frame.extcontext.lsx.context.regs,
                   sizeof(ext_ctx->v));
            ext_ctx->fcc      = frame.extcontext.lsx.context.fcc;
            ext_ctx->fcsr     = frame.extcontext.lsx.context.fcsr;
            ext_ctx->reserved = 0;
        }

#endif

        [[nodiscard]]
        bool is_user_handler(const SigAction &action) noexcept {
            return action.handler != LINUX_SIG_DFL &&
                   action.handler != LINUX_SIG_IGN;
        }

        [[nodiscard]]
        bool should_deliver_signal(const TCB &tcb) noexcept {
            return tcb.task != nullptr && tcb.task->is_linux_process &&
                   !tcb.signal_delivery_active;
        }

        [[nodiscard]]
        Result<size_t> choose_deliverable_signal(PCB &pcb) noexcept {
            auto pending = pcb.signal_state.pending_mask.load() &
                           ~pcb.signal_state.blocked_mask.load();
            if (pending == 0) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return first_signal_index(pending);
        }
    }  // namespace

    Result<void> deliver_signal_if_needed(util::nonnull<TCB *> tcb,
                                          util::nonnull<Context *> ctx) noexcept {
        if (!should_deliver_signal(*tcb)) {
            void_return();
        }

        auto *pcb = tcb->task;
        assert(pcb != nullptr);

        auto signo_res = choose_deliverable_signal(*pcb);
        if (!signo_res.has_value()) {
            void_return();
        }
        size_t signo = signo_res.value();
        if (signo >= SignalState::MAX_SIGNALS) {
            void_return();
        }

        auto &state  = pcb->signal_state;
        auto action  = state.actions[signo];
        uint64_t bit = uint64_t(1) << signo;

        if (action.handler == LINUX_SIG_IGN) {
            state.pending_mask &= ~bit;
            void_return();
        }

        if (action.handler == LINUX_SIG_DFL) {
            state.pending_mask &= ~bit;
            if (is_default_ignored_signal(signo)) {
                void_return();
            }
            if (is_default_terminating_signal(signo)) {
                auto kill_res = TaskManager::inst().kill_pcb_impl(
                    pcb, tcb.get(), default_exit_code_for_signal(signo));
                propagate(kill_res);
            }
            void_return();
        }

        if (!is_user_handler(action)) {
            void_return();
        }
        if ((action.flags & LINUX_SA_RESTORER) == 0 || action.restorer == 0) {
            loggers::TASK::ERROR(
                "signal delivery 缺少可用 restorer: pid=%lu tid=%lu signo=%lu flags=0x%lx restorer=%p",
                pcb->pid, tcb->tid, static_cast<unsigned long>(signo),
                static_cast<unsigned long>(action.flags),
                reinterpret_cast<void *>(action.restorer));
            void_return();
        }

        state.pending_mask &= ~bit;
        uint64_t old_mask  = state.blocked_mask.load();
        uint64_t new_mask  = old_mask | action.mask | bit;
        state.blocked_mask = new_mask;

#if defined(__ARCH_riscv64__)
        linux_riscv64_frame frame{};
        fill_user_siginfo(frame.info, signo);
        bool from_syscall = tcb->syscall_info.executing() ||
                            tcb->syscall_info.completed();
        if (from_syscall) {
            frame.flags |= LINUX_FRAME_FROM_SYSCALL;
            frame.syscall_pc = ctx->pc();
        }
        if (tcb->ext_ctx_live && tcb->ext_ctx != nullptr) {
            save_ext_context(*tcb->ext_ctx);
        }
        fill_ucontext(frame.ucontext, *ctx, old_mask,
                      tcb->ext_ctx.get());

        addr_t frame_addr =
            (ctx->sp() - sizeof(frame)) & ~static_cast<addr_t>(0xFUL);
        if (frame_addr == 0) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        syscall::UBuffer frame_buf(VirAddr(frame_addr), sizeof(frame));
        memcpy(frame_buf.kbuf(), &frame, sizeof(frame));
        auto commit_res = frame_buf.commit_to_user();
        propagate(commit_res);

        tcb->signal_delivery_active = true;
        tcb->signal_delivery_signo  = signo;
        tcb->signal_frame_user_sp   = frame_addr;

        ctx->pc() = action.handler;
        ctx->sp() = frame_addr;
        ctx->ra() = action.restorer;
        ctx->a0 = signo;
        ctx->a1 = (action.flags & LINUX_SA_SIGINFO) != 0
                      ? frame_addr + offsetof(linux_riscv64_frame, info)
                      : 0;
        ctx->a2 = (action.flags & LINUX_SA_SIGINFO) != 0
                      ? frame_addr + offsetof(linux_riscv64_frame, ucontext)
                      : 0;
        ctx->sstatus.value = LINUX_USER_STATUS_BITS;
#elif defined(__ARCH_loongarch64__)
        linux_loongarch64_frame frame{};
        fill_user_siginfo(frame.info, signo);
        bool from_syscall = tcb->syscall_info.executing() ||
                            tcb->syscall_info.completed();
        if (from_syscall) {
            frame.flags |= LINUX_FRAME_FROM_SYSCALL;
            frame.syscall_pc = ctx->pc();
        }
        if (tcb->ext_ctx_live && tcb->ext_ctx != nullptr) {
            save_ext_context(*tcb->ext_ctx);
        }
        fill_ucontext(frame.ucontext, *ctx, old_mask,
                      tcb->ext_ctx.get());
        fill_extcontext(frame.extcontext, tcb->ext_ctx.get());

        addr_t frame_addr =
            (ctx->sp() - sizeof(frame)) & ~static_cast<addr_t>(0xFUL);
        if (frame_addr == 0) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        syscall::UBuffer frame_buf(VirAddr(frame_addr), sizeof(frame));
        memcpy(frame_buf.kbuf(), &frame, sizeof(frame));
        auto commit_res = frame_buf.commit_to_user();
        propagate(commit_res);

        tcb->signal_delivery_active = true;
        tcb->signal_delivery_signo  = signo;
        tcb->signal_frame_user_sp   = frame_addr;

        ctx->pc() = action.handler;
        ctx->sp() = frame_addr;
        ctx->ra() = action.restorer;
        ctx->a0 = signo;
        ctx->a1 = (action.flags & LINUX_SA_SIGINFO) != 0
                      ? frame_addr + offsetof(linux_loongarch64_frame, info)
                      : 0;
        ctx->a2 = (action.flags & LINUX_SA_SIGINFO) != 0
                      ? frame_addr + offsetof(linux_loongarch64_frame, ucontext)
                      : 0;
        ctx->prmd.pplv = PLV_USER;
        ctx->prmd.pie  = 1;
#endif
        void_return();
    }

    Result<size_t> rt_sigreturn_current(util::nonnull<TCB *> tcb,
                                        util::nonnull<Context *> ctx) noexcept {
        if (tcb->task == nullptr || !tcb->task->is_linux_process) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (!tcb->signal_delivery_active || tcb->signal_frame_user_sp == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if ((ctx->sp() & 0xFUL) != 0) {
            (void)TaskManager::inst().kill_pcb_impl(
                tcb->task, tcb.get(), default_exit_code_for_signal(LINUX_SIGSEGV));
            unexpect_return(ErrCode::FAILURE);
        }

#if defined(__ARCH_riscv64__)
        linux_riscv64_frame frame{};
        syscall::UBuffer frame_buf(VirAddr(ctx->sp()), sizeof(frame));
        auto sync_res = frame_buf.sync_from_user();
        if (!sync_res.has_value()) {
            (void)TaskManager::inst().kill_pcb_impl(
                tcb->task, tcb.get(),
                default_exit_code_for_signal(LINUX_SIGSEGV));
            propagate_return(sync_res);
        }
        memcpy(&frame, frame_buf.kbuf(), sizeof(frame));
        restore_context(*ctx, frame.ucontext);
        if ((frame.flags & LINUX_FRAME_FROM_SYSCALL) != 0 &&
            ctx->pc() == frame.syscall_pc)
        {
            ctx->pc() += LINUX_SYSCALL_INS_LEN;
        }
        restore_ext(frame.ucontext, tcb->ext_ctx.get());
        if (tcb->ext_ctx != nullptr) {
            restore_ext_context(*tcb->ext_ctx);
            tcb->ext_ctx_live = true;
        }
        tcb->task->signal_state.blocked_mask =
            user_mask_to_kernel_mask(frame.ucontext.uc_sigmask.sig[0]);
        size_t result = ctx->a0;
#elif defined(__ARCH_loongarch64__)
        linux_loongarch64_frame frame{};
        syscall::UBuffer frame_buf(VirAddr(ctx->sp()), sizeof(frame));
        auto sync_res = frame_buf.sync_from_user();
        if (!sync_res.has_value()) {
            (void)TaskManager::inst().kill_pcb_impl(
                tcb->task, tcb.get(),
                default_exit_code_for_signal(LINUX_SIGSEGV));
            propagate_return(sync_res);
        }
        memcpy(&frame, frame_buf.kbuf(), sizeof(frame));
        restore_context(*ctx, frame.ucontext);
        if ((frame.flags & LINUX_FRAME_FROM_SYSCALL) != 0 &&
            ctx->pc() == frame.syscall_pc)
        {
            ctx->pc() += LINUX_SYSCALL_INS_LEN;
        }
        restore_ext(frame, tcb->ext_ctx.get());
        if (tcb->ext_ctx != nullptr) {
            restore_ext_context(*tcb->ext_ctx);
            tcb->ext_ctx_live = true;
        }
        tcb->task->signal_state.blocked_mask =
            user_mask_to_kernel_mask(frame.ucontext.uc_sigmask.sig[0]);
        size_t result = ctx->a0;
#endif
        reset_tcb_signal_delivery(*tcb);
        return result;
    }
}  // namespace task
