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
#include <env.h>
#include <logger.h>
#include <sbi/sbi.h>

namespace device {
    /**
     * @brief 使用统一设备节点创建 CPU 本地中断控制器后端.
     */
    Result<util::owner<RiscVIntC *>> RiscVIntC::create(
        util::owner<DeviceNode *> node, std::string name, intc_t identifier,
        cpuid_t hart_id) noexcept {
        if (node.get() == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC 创建失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *chip = new RiscVIntC(std::move(node), std::move(name), identifier,
                                   hart_id);
        if (chip == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        loggers::INTERRUPT::DEBUG("创建 RiscVIntC: id=%u hart=%u name=%s",
                                  identifier, hart_id, chip->name());
        return util::owner<RiscVIntC *>(chip);
    }

    /**
     * @brief 获取 CPU 本地中断控制器的 MMIO 区域列表.
     */
    std::vector<PhyArea> RiscVIntC::mmio_regions() const {
        if (_node.get() == nullptr) {
            return {};
        }

        auto regions = _node->mmio_regions();
        if (!regions.has_value()) {
            return {};
        }
        return regions.value();
    }

    /**
     * @brief 读取节点 MMIO 区域列表.
     */
    Result<std::vector<PhyArea>> Clint::load_mmio_regions(
        const DeviceNode &node) noexcept {
        auto mmio_res = node.mmio_regions();
        if (!mmio_res.has_value() || mmio_res->empty()) {
            loggers::INTERRUPT::ERROR("Clint 节点缺少 mmio 区域");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return mmio_res.value();
    }

    /**
     * @brief 读取节点 CLINT 所需的 software/timer virq.
     */
    Result<std::pair<virq_t, virq_t>> Clint::load_clint_virqs(
        const DeviceNode &node) noexcept {
        auto virqs_res = node.irqs();
        if (!virqs_res.has_value()) {
            loggers::INTERRUPT::ERROR("Clint 节点缺少 irqs 属性");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (virqs_res->size() < 2) {
            loggers::INTERRUPT::ERROR("Clint 节点 irqs 数量不足: %u",
                                      static_cast<unsigned>(virqs_res->size()));
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (virqs_res->size() > 2) {
            loggers::INTERRUPT::DEBUG(
                "Clint 节点 irqs 数量超过 2, 仅使用前两项: count=%u",
                static_cast<unsigned>(virqs_res->size()));
        }
        return std::pair<virq_t, virq_t>{virqs_res->at(0), virqs_res->at(1)};
    }

    /**
     * @brief 由统一设备节点构造 CLINT 控制器对象.
     */
    Clint::Clint(util::owner<DeviceNode *> node, std::string name,
                 intc_t identifier, cpuid_t hart_id,
                 std::vector<cpuid_t> target_harts) noexcept
        : _node(std::move(node)),
          _name(std::move(name)),
          _identifier(identifier),
          _hart_id(hart_id),
          _target_harts(std::move(target_harts)) {}

    /**
     * @brief 由统一设备节点创建 RISC-V CLINT 后端.
     */
    Result<util::owner<Clint *>> Clint::create(
        util::owner<DeviceNode *> node, std::string name, intc_t identifier,
        cpuid_t hart_id, std::vector<cpuid_t> target_harts) noexcept {
        if (node.get() == nullptr) {
            loggers::INTERRUPT::ERROR("Clint 创建失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto mmio_res = load_mmio_regions(*node);
        propagate(mmio_res);
        auto virq_pair_res = load_clint_virqs(*node);
        propagate(virq_pair_res);

        auto *chip = new Clint(std::move(node), std::move(name), identifier,
                               hart_id, std::move(target_harts));
        if (chip == nullptr) {
            loggers::INTERRUPT::ERROR("Clint 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        chip->_mmio_regions   = std::move(mmio_res.value());
        chip->_software_virq  = virq_pair_res.value().first;
        chip->_clock_virq     = virq_pair_res.value().second;
        loggers::INTERRUPT::DEBUG(
            "创建 Clint: id=%u hart=%u target_harts=%u sw_virq=%llu clock_virq=%llu",
            identifier, hart_id, static_cast<unsigned>(chip->_target_harts.size()),
            static_cast<unsigned long long>(chip->_software_virq),
            static_cast<unsigned long long>(chip->_clock_virq));
        return util::owner<Clint *>(chip);
    }

    /**
     * @brief 读取节点 MMIO 区域列表.
     */
    Result<std::vector<PhyArea>> Plic::load_mmio_regions(
        const DeviceNode &node) noexcept {
        auto mmio_res = node.mmio_regions();
        if (!mmio_res.has_value() || mmio_res->empty()) {
            loggers::INTERRUPT::ERROR("Plic 节点缺少 mmio 区域");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return mmio_res.value();
    }

    /**
     * @brief 使用统一设备节点构造 PLIC 对象.
     */
    Plic::Plic(util::owner<DeviceNode *> node, std::string name,
               intc_t identifier, hwirq_t source_count,
               std::vector<PlicContext> contexts) noexcept
        : _name(std::move(name)),
          _identifier(identifier),
          _node(std::move(node)),
          _source_count(source_count),
          _contexts(std::move(contexts)) {}

    /**
     * @brief 创建一个 PLIC 控制器后端.
     */
    Result<util::owner<Plic *>> Plic::create(util::owner<DeviceNode *> node,
                                             std::string name, intc_t identifier,
                                             hwirq_t source_count,
                                             std::vector<PlicContext> contexts) noexcept {
        if (node.get() == nullptr) {
            loggers::INTERRUPT::ERROR("Plic 创建失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto mmio_res = load_mmio_regions(*node);
        propagate(mmio_res);

        auto *chip = new Plic(std::move(node), std::move(name), identifier,
                              source_count, std::move(contexts));
        if (chip == nullptr) {
            loggers::INTERRUPT::ERROR("Plic 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        chip->_mmio_regions = std::move(mmio_res.value());
        if (!chip->_mmio_regions.empty()) {
            auto base = convert<KpaAddr>(chip->_mmio_regions.front().begin);
            chip->_base = reinterpret_cast<volatile sus_u32 *>(base.addr());
        }

        for (const auto &context : chip->_contexts) {
            chip->_context_indices[context.hart_id] = context.context_index;
            chip->write32(threshold_offset(context.context_index), 0);
        }

        loggers::INTERRUPT::DEBUG(
            "创建 Plic: id=%u source_count=%u contexts=%u name=%s",
            identifier, static_cast<unsigned>(source_count),
            static_cast<unsigned>(chip->_contexts.size()), chip->name());
        return util::owner<Plic *>(chip);
    }

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
            case EXTERNAL_LOCAL_IRQ:
                sie.seie = 1;
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
            case EXTERNAL_LOCAL_IRQ:
                sie.seie = 0;
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

    const char *Plic::name() const noexcept {
        return _name.c_str();
    }

    std::vector<PhyArea> Plic::mmio_regions() const {
        return _mmio_regions;
    }

    Result<const PlicContext &> Plic::context_for_hart(cpuid_t hart_id) const noexcept {
        for (const auto &context : _contexts) {
            if (context.hart_id == hart_id) {
                return std::cref(context);
            }
        }
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<void> Plic::validate_source(hwirq_t hw_irq) const noexcept {
        if (hw_irq == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (hw_irq > _source_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        void_return();
    }

    sus_u32 Plic::read32(size_t offset) const noexcept {
        assert(_base != nullptr);
        return *reinterpret_cast<volatile sus_u32 *>(
            reinterpret_cast<addr_t>(const_cast<sus_u32 *>(_base)) + offset);
    }

    void Plic::write32(size_t offset, sus_u32 value) noexcept {
        assert(_base != nullptr);
        *reinterpret_cast<volatile sus_u32 *>(
            reinterpret_cast<addr_t>(const_cast<sus_u32 *>(_base)) + offset) =
            value;
    }

    Result<void> Plic::enable_irq(hwirq_t hw_irq) {
        auto valid_res = validate_source(hw_irq);
        propagate(valid_res);

        for (const auto &context : _contexts) {
            size_t offset = enable_offset(context.context_index, hw_irq);
            sus_u32 mask  = static_cast<sus_u32>(1u << (hw_irq % 32));
            sus_u32 value = read32(offset) | mask;
            write32(offset, value);
        }
        void_return();
    }

    Result<void> Plic::disable_irq(hwirq_t hw_irq) {
        auto valid_res = validate_source(hw_irq);
        propagate(valid_res);

        for (const auto &context : _contexts) {
            size_t offset = enable_offset(context.context_index, hw_irq);
            sus_u32 mask  = static_cast<sus_u32>(1u << (hw_irq % 32));
            sus_u32 value = read32(offset) & ~mask;
            write32(offset, value);
        }
        void_return();
    }

    Result<void> Plic::set_priority(hwirq_t hw_irq, irq_prio_t prio) {
        auto valid_res = validate_source(hw_irq);
        propagate(valid_res);
        write32(priority_offset(hw_irq), prio);
        void_return();
    }

    Result<void> Plic::set_affinity(hwirq_t hw_irq, cpu_mask_t mask) {
        (void)hw_irq;
        (void)mask;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Plic::ack_irq(hwirq_t hw_irq) {
        auto valid_res = validate_source(hw_irq);
        propagate(valid_res);

        if (env::hart_ctx == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        cpuid_t hart_id = static_cast<cpuid_t>(env::hart_ctx->hart_id());
        auto context_res = context_for_hart(hart_id);
        propagate(context_res);

        auto claimed_it = _claimed_sources.find(hart_id);
        if (claimed_it == _claimed_sources.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (claimed_it->second != hw_irq) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        write32(claim_complete_offset(context_res.value().get().context_index),
                hw_irq);
        _claimed_sources.erase(claimed_it);
        void_return();
    }

    Result<void> Plic::set_trigger(hwirq_t hw_irq, IrqTrigger trigger) {
        (void)hw_irq;
        (void)trigger;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<hwirq_t> Plic::resolve_claim_for_current_hart() noexcept {
        if (env::hart_ctx == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        cpuid_t hart_id = static_cast<cpuid_t>(env::hart_ctx->hart_id());
        auto context_res = context_for_hart(hart_id);
        propagate(context_res);

        auto source = static_cast<hwirq_t>(read32(
            claim_complete_offset(context_res.value().get().context_index)));
        if (source == 0) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto valid_res = validate_source(source);
        propagate(valid_res);
        _claimed_sources[hart_id] = source;
        return source;
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
