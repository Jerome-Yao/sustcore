/**
 * @file exception.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 异常处理程序
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/int/exception.h>
#include <arch/riscv64/int/trap.h>

#include <basec/logger.h>

void general_isr(void);
void general_exception(umb_t scause, umb_t sepc, umb_t stval,
                       InterruptContextRegisterList *reglist_ptr);

#if IVT_MODE == VECTORED
__attribute__((aligned(4), section(".text")))
dword IVT[IVT_ENTRIES] = {};

static dword emit_j_ins(const dword offset) {
    if (offset & 0x3) {
        // 偏移量必须是4字节对齐的
        return 0;
    }

    const dword j_opcode = 0x6F;
    const dword imm20    = (offset >> 20) & 0x1;
    const dword imm10_1  = (offset >> 1 ) & 0x3FF;
    const dword imm11    = (offset >> 11) & 0x1;
    const dword imm19_12 = (offset >> 12) & 0xFF;

    const dword imm =
        (imm20    << 31 )|
        (imm10_1  << 21 )|
        (imm11    << 20 )|
        (imm19_12 << 12 );

    return imm | j_opcode;
}

static dword emit_ivt_entry(ISRService isr_func, int idx) {
    qword q_off = (qword)isr_func - (qword)IVT - (idx * sizeof(dword));
    return emit_j_ins((dword)q_off);
}
#endif

void init_ivt() {
#if IVT_MODE == VECTORED
    // 设置为vectored模式
    umb_t ivt_addr = (umb_t)IVT;
    if (ivt_addr & 0x3) {
        log_info("错误: IVT地址未对齐!");
        return;
    }
    umb_t stvec = (ivt_addr & ~0b11) | 0b01;

    for (int i = 0 ; i < IVT_ENTRIES; i++) {
        IVT[i] = emit_ivt_entry(general_isr, i);
    }

    log_info("general_isr 地址: 0x%lx", (umb_t)general_isr);
    log_info("general_exception 地址: 0x%lx", (umb_t)general_exception);
    log_info("IVT 地址: 0x%lx", (umb_t)IVT);
#elif IVT_MODE == DIRECT
    // 采用direct模式
    umb_t stvec = (umb_t)general_isr;
    if (stvec & 0x3) {
        log_info("错误: stvec地址未对齐!");
        return;
    }
    log_info("general_isr 地址: 0x%lx", (umb_t)general_isr);
#endif

    asm volatile("csrw stvec, %0" : : "r"(stvec));
}

#define RISCV_CPU_INTERRUPT_MASK (1ull << 63)

ISR_SERVICE_ATTRIBUTE
void general_isr(void) {
    ISR_SERVICE_START(general_isr, 128);


    if (scause & RISCV_CPU_INTERRUPT_MASK) {
        log_info("这是一个中断");
    } else {
        general_exception(scause, sepc, stval, reglist_ptr);
    }

    ISR_SERVICE_END(general_isr);
}

enum {
    EXCEPTION_INST_MISALIGNED     = 0,   // 指令地址不对齐
    EXCEPTION_INST_ACCESS_FAULT   = 1,   // 指令访问错误
    EXCEPTION_ILLEGAL_INST        = 2,   // 非法指令
    EXCEPTION_BREAKPOINT          = 3,   // 断点
    EXCEPTION_LOAD_MISALIGNED     = 4,   // 加载地址不对齐
    EXCEPTION_LOAD_ACCESS_FAULT   = 5,   // 加载访问错误
    EXCEPTION_STORE_MISALIGNED    = 6,   // 存储地址不对齐
    EXCEPTION_STORE_ACCESS_FAULT  = 7,   // 存储访问错误
    EXCEPTION_ECALL_U             = 8,   // 用户模式环境调用
    EXCEPTION_ECALL_S             = 9,   // 监管模式环境调用
    EXCEPTION_ECALL_M             = 11,  // 机器模式环境调用
    EXCEPTION_INST_PAGE_FAULT     = 12,  // 指令页错误
    EXCEPTION_LOAD_PAGE_FAULT     = 13,  // 加载页错误
    EXCEPTION_STORE_PAGE_FAULT    = 15   // 存储页错误
};



void general_exception(umb_t scause, umb_t sepc, umb_t stval,
                       InterruptContextRegisterList *reglist_ptr)
{
    log_info("异常处理程序被调用!");
    log_info("scause: 0x%lx, sepc: 0x%lx, stval: 0x%lx", scause, sepc, stval);
    log_info("reglist_ptr: 0x%lx", reglist_ptr);

    for (int i = 0; i < 31; i++) {
        log_info("x%d: 0x%lx", i + 1, reglist_ptr->regs[i]);
    }

    log_info("sepc: 0x%lx", reglist_ptr->sepc);
    log_info("sstatus: 0x%lx", reglist_ptr->sstatus);

    if ( (reglist_ptr->sstatus >> 8) & 0x1 ) {
        log_info("异常发生在S-Mode");
    } else {
        log_info("异常发生在U-Mode");
    }

    const char *exception_msg[] = {
        "指令地址不对齐",
        "指令访问错误",
        "非法指令",
        "断点",
        "加载地址不对齐",
        "加载访问错误",
        "存储地址不对齐",
        "存储访问错误",
        "用户模式环境调用",
        "监管模式环境调用",
        "保留",
        "机器模式环境调用",
        "指令页错误",
        "加载页错误",
        "保留",
        "存储页错误"
    };

    if (scause < sizeof(exception_msg)/sizeof(exception_msg[0])) {
        log_info("异常类型: %s (%lu)", exception_msg[scause], scause);
    } else {
        log_info("异常类型: 未知 (%lu)", scause);
    }

    if (scause == 2) {
        // 跳过该指令!
        reglist_ptr->sepc += 4;
    }
    else {
        log_info("未知异常类型: 0x%lx", scause);
    }
}