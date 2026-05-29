/**
 * @file int.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief interrupt
 * @version alpha-1.0.0
 * @date 2026-05-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/csr.h>
#include <device/int.h>
#include <device/model.h>
#include <sbi/sbi.h>

namespace device {
    Result<void> RiscVIntC::enable_irq(hwirq_t hw_irq) {
        csr_sie_t sie = csr_get_sie();
        switch (hw_irq) {
            case SOFTWARE_LOCAL_IRQ:
                sie.ssie = 1;
                csr_set_sie(sie);
                void_return();
            case CLOCK_LOCAL_IRQ:
                sie.stie = 1;
                csr_set_sie(sie);
                void_return();
            default: unexpect_return(ErrCode::NOT_SUPPORTED);
        }
    }

    Result<void> RiscVIntC::disable_irq(hwirq_t hw_irq) {
        csr_sie_t sie = csr_get_sie();
        switch (hw_irq) {
            case SOFTWARE_LOCAL_IRQ:
                sie.ssie = 0;
                csr_set_sie(sie);
                void_return();
            case CLOCK_LOCAL_IRQ:
                sie.stie = 0;
                csr_set_sie(sie);
                void_return();
            default: unexpect_return(ErrCode::NOT_SUPPORTED);
        }
    }

    Result<void> RiscVIntC::set_priority(hwirq_t hw_irq, irq_prio_t prio) {
        (void)hw_irq;
        (void)prio;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> RiscVIntC::set_affinity(hwirq_t hw_irq, cpu_mask_t mask) {
        (void)hw_irq;
        (void)mask;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> RiscVIntC::ack_irq(hwirq_t hw_irq) {
        (void)hw_irq;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> RiscVIntC::set_trigger(hwirq_t hw_irq, IrqTrigger trigger) {
        (void)hw_irq;
        (void)trigger;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    /**
     * @brief 编程下一次 CLINT 定时器事件.
     */
    void ClintAlarm::set_next_event(units::time delta) noexcept {
        // 获取当前 mtime，计算绝对计数值，调用 SBI
        units::tick now  = _clksrc->now();
        units::tick gaps = delta * _clksrc->frequency();
        sbi_legacy_set_timer(now + gaps);
    }

    ClintAlarm::ClintAlarm(ClockSource *clksrc, virq_t clock_virq) noexcept
        : Alarm(clksrc), _clock_virq(clock_virq) {
        _last_recorded_time = _clksrc->to_ns(_clksrc->now());
        assert(DeviceModel::initialized());
        auto &irqman      = DeviceModel::inst().interrupt();
        auto register_res = irqman.register_handler(
            clock_virq, this_call(this, &ClintAlarm::handle_irq));
        assert(register_res.has_value());
    }

    void ClintAlarm::handle_irq(const IrqEvent &event) noexcept {
        if (event.virq != _clock_virq) {
            loggers::INTERRUPT::ERROR(
                "ClintAlarm 收到不匹配的 virq: got=%llu expect=%llu",
                static_cast<unsigned long long>(event.virq),
                static_cast<unsigned long long>(_clock_virq));
            return;
        }
        units::time now = _clksrc->to_ns(_clksrc->now());
        if (_handler) {
            _handler(ClockEvent{.last = _last_recorded_time, .now = now});
        }
        _last_recorded_time = now;
    }

    /**
     * @brief 构造一个 CLINT 控制器对象.
     */
    Clint::Clint(std::string name, intc_t identifier,
                 std::vector<PhyArea> mmio_regions, cpuid_t hart_id,
                 std::vector<cpuid_t> target_harts, std::vector<virq_t> virqs) noexcept
        : _name(std::move(name)),
          _identifier(identifier),
          _mmio_regions(std::move(mmio_regions)),
          _hart_id(hart_id),
          _target_harts(std::move(target_harts)),
          _software_virq(virqs[0]),
          _clock_virq(virqs[1]) {}

    /**
     * @brief 获取控制器名称.
     */
    const char *Clint::name() const noexcept {
        return _name.c_str();
    }

    /**
     * @brief 获取 MMIO 区域列表.
     */
    std::vector<PhyArea> Clint::mmio_regions() const {
        return _mmio_regions;
    }

    /**
     * @brief 使能本地中断.
     */
    Result<void> Clint::enable_irq(hwirq_t hw_irq) {
        loggers::INTERRUPT::ERROR("Clint 不支持启用 irq", _identifier, hw_irq);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    /**
     * @brief 屏蔽本地中断.
     */
    Result<void> Clint::disable_irq(hwirq_t hw_irq) {
        loggers::INTERRUPT::ERROR("Clint[%u] 不支持关闭 hwirq", _identifier,
                                  hw_irq);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    /**
     * @brief 设置中断优先级.
     */
    Result<void> Clint::set_priority(hwirq_t hw_irq, irq_prio_t prio) {
        loggers::INTERRUPT::DEBUG(
            "Clint[%u] set_priority hwirq=%u prio=%u 不支持", _identifier,
            hw_irq, prio);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    /**
     * @brief 设置中断亲和性.
     */
    Result<void> Clint::set_affinity(hwirq_t hw_irq, cpu_mask_t mask) {
        loggers::INTERRUPT::ERROR(
            "Clint[%u] set_affinity hwirq=%u mask=0x%llx 不支持", _identifier,
            hw_irq, static_cast<unsigned long long>(mask));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Clint::ack_irq(hwirq_t hw_irq) {
        loggers::INTERRUPT::DEBUG("Clint[%u] ack_irq hwirq=%u 不支持",
                                  _identifier, hw_irq);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Clint::set_trigger(hwirq_t hw_irq, IrqTrigger trigger) {
        loggers::INTERRUPT::DEBUG(
            "Clint[%u] set_trigger hwirq=%u trigger=%d 不支持", _identifier,
            hw_irq, static_cast<int>(trigger));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    bool Clint::supports_hart(cpuid_t hart_id) const noexcept {
        for (cpuid_t target : _target_harts) {
            if (target == hart_id) {
                return true;
            }
        }
        return false;
    }
}  // namespace device
