/**
 * @file int.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief intterrupt
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <device/clock.h>
#include <logger.h>
#include <sus/owner.h>
#include <sus/units.h>
#include <device/device.h>
#include <sustcore/errcode.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class InterruptGuard {
private:
    bool entered      = false;
    bool prev_enabled = false;

public:
    InterruptGuard() = default;

    /**
     * @brief 进入中断关闭保护区.
     */
    void enter() {
        if (entered) {
            return;
        }
        prev_enabled = Interrupt::enabled();
        Interrupt::cli();
        entered = true;
    }

    /**
     * @brief 退出保护区并恢复中断状态.
     */
    ~InterruptGuard() {
        if (entered && prev_enabled) {
            Interrupt::sti();
        }
    }
};
namespace device {

    /**
     * @brief 中断触发方式.
     */
    enum class IrqTrigger { EDGE_RISING, EDGE_FALLING, LEVEL_HIGH, LEVEL_LOW };

    /**
     * @brief 中断控制器抽象接口.
     */
    class IrqChip {
    public:
        virtual ~IrqChip() = default;
        [[nodiscard]]
        virtual const char *name() const = 0;
        [[nodiscard]]
        virtual std::vector<PhyArea> mmio_regions() const = 0;
        [[nodiscard]]
        virtual Result<void> enable_irq(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> disable_irq(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> set_priority(hwirq_t hw_irq, irq_prio_t prio) = 0;
        [[nodiscard]]
        virtual Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) = 0;
        [[nodiscard]]
        virtual Result<void> ack_irq(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> set_trigger(hwirq_t hw_irq,
                                         IrqTrigger trigger) = 0;
    };

    /**
     * @brief 中断域与硬件中断号的解析结果.
     */
    struct IrqResolveResult {
        domain_t domain = INVALID_DOMAIN_ID;
        hwirq_t hw_irq  = 0;
    };

    /**
     * @brief 一次 virq 分发时传递给处理函数的上下文.
     */
    struct IrqEvent {
        virq_t virq    = 0;
        hwirq_t hw_irq = 0;
        umb_t scause   = 0;
        umb_t sepc     = 0;
        umb_t stval    = 0;
        void *context  = nullptr;
    };

    /**
     * @brief 中断域抽象接口.
     */
    class IrqDomain {
    public:
        virtual ~IrqDomain() = default;
        [[nodiscard]]
        virtual domain_t id() const noexcept = 0;
        [[nodiscard]]
        virtual const char *name() const noexcept = 0;
        [[nodiscard]]
        virtual IrqChip &chip() const noexcept = 0;
        [[nodiscard]]
        virtual Result<virq_t> bind(hwirq_t hw_irq, virq_t virq) = 0;
        [[nodiscard]]
        virtual Result<virq_t> to_virq(hwirq_t hw_irq) const = 0;
        [[nodiscard]]
        virtual Result<hwirq_t> to_hwirq(virq_t virq) const = 0;
        [[nodiscard]]
        virtual bool contains(virq_t virq) const noexcept = 0;
        [[nodiscard]]
        virtual bool supports(hwirq_t hw_irq) const noexcept = 0;
        [[nodiscard]]
        virtual Result<void> enable(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> disable(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> ack(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> set_priority(hwirq_t hw_irq, irq_prio_t prio) = 0;
        [[nodiscard]]
        virtual Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) = 0;
        [[nodiscard]]
        virtual Result<void> set_trigger(hwirq_t hw_irq,
                                         IrqTrigger trigger) = 0;
    };

    /**
     * @brief 线性 hw_irq 编号的固定大小中断域.
     *
     * @tparam MAX_HW_IRQ 域内可支持的最大硬件中断号数量.
     */
    template <size_t MAX_HW_IRQ>
    class LinearIrqDomain final : public IrqDomain {
    public:
        /**
         * @brief 构造一个线性中断域.
         *
         * @param identifier 域 ID.
         * @param domain_name 域名称.
         * @param chip 后端中断芯片所有权.
         */
        LinearIrqDomain(domain_t identifier, std::string domain_name,
                        util::owner<IrqChip *> chip) noexcept
            : _id(identifier),
              _name(std::move(domain_name)),
              _chip(std::move(chip)),
              _virq_to_hwirq() {
            for (auto &slot : _virqs) {
                slot = std::nullopt;
            }
        }

        [[nodiscard]]
        domain_t id() const noexcept override {
            return _id;
        }

        [[nodiscard]]
        const char *name() const noexcept override {
            return _name.c_str();
        }

        [[nodiscard]]
        IrqChip &chip() const noexcept override {
            return *_chip;
        }

        [[nodiscard]]
        Result<virq_t> bind(hwirq_t hw_irq, virq_t virq) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            auto &slot = _virqs[hw_irq];
            if (slot.has_value()) {
                if (*slot != virq) {
                    unexpect_return(ErrCode::KEY_DUPLICATED);
                }
                return *slot;
            }
            if (_virq_to_hwirq.contains(virq)) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }

            slot                 = virq;
            _virq_to_hwirq[virq] = hw_irq;
            return virq;
        }

        [[nodiscard]]
        Result<virq_t> to_virq(hwirq_t hw_irq) const override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (!_virqs[hw_irq].has_value()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return *_virqs[hw_irq];
        }

        [[nodiscard]]
        Result<hwirq_t> to_hwirq(virq_t virq) const override {
            auto it = _virq_to_hwirq.find(virq);
            if (it == _virq_to_hwirq.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return it->second;
        }

        [[nodiscard]]
        bool contains(virq_t virq) const noexcept override {
            return _virq_to_hwirq.contains(virq);
        }

        [[nodiscard]]
        bool supports(hwirq_t hw_irq) const noexcept override {
            return static_cast<size_t>(hw_irq) < MAX_HW_IRQ;
        }

        [[nodiscard]]
        Result<void> enable(hwirq_t hw_irq) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->enable_irq(hw_irq);
        }

        [[nodiscard]]
        Result<void> disable(hwirq_t hw_irq) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->disable_irq(hw_irq);
        }

        [[nodiscard]]
        Result<void> ack(hwirq_t hw_irq) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->ack_irq(hw_irq);
        }

        [[nodiscard]]
        Result<void> set_priority(hwirq_t hw_irq, irq_prio_t prio) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->set_priority(hw_irq, prio);
        }

        [[nodiscard]]
        Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->set_affinity(hw_irq, mask);
        }

        [[nodiscard]]
        Result<void> set_trigger(hwirq_t hw_irq, IrqTrigger trigger) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->set_trigger(hw_irq, trigger);
        }

    private:
        domain_t _id = INVALID_DOMAIN_ID;
        std::string _name;
        util::owner<IrqChip *> _chip = util::owner<IrqChip *>(nullptr);
        std::optional<virq_t> _virqs[MAX_HW_IRQ]{};
        std::unordered_map<virq_t, hwirq_t> _virq_to_hwirq;
    };

    /**
     * @brief 全局中断管理器.
     */
    class IrqManager {
    private:
        using IrqHandler = std::function<void(const IrqEvent &)>;

        std::unordered_map<domain_t, util::owner<IrqDomain *>> _domains;
        std::unordered_map<virq_t, IrqResolveResult> _virq_map;
        std::unordered_map<virq_t, IrqHandler> _handlers;
        virq_t _next_virq = 1;

    public:
        IrqManager() = default;

        /**
         * @brief 注册一个中断域.
         */
        Result<void> register_domain(util::owner<IrqDomain *> domain) {
            if (domain == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (_domains.find(domain->id()) != _domains.end()) {
                loggers::INTERRUPT::ERROR("中断域 ID: %u 已存在!",
                                          domain->id());
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
            loggers::INTERRUPT::INFO("注册中断域: %s (ID: %u)", domain->name(),
                                     domain->id());
            _domains[domain->id()] = std::move(domain);
            void_return();
        }

        /**
         * @brief 为指定域中的 hw_irq 分配稳定 virq.
         *
         * @param domain_id 中断域 ID.
         * @param hw_irq 域内硬件中断号.
         * @return Result<virq_t> 对应的全局 virq.
         */
        [[nodiscard]]
        Result<virq_t> allocate_virq(domain_t domain_id, hwirq_t hw_irq) {
            auto domain_res = get_domain(domain_id);
            propagate(domain_res);

            auto current_res = domain_res.value().get().to_virq(hw_irq);
            if (current_res.has_value()) {
                return current_res.value();
            }
            if (current_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                propagate_return(current_res);
            }

            virq_t virq   = _next_virq++;
            auto bind_res = domain_res.value().get().bind(hw_irq, virq);
            propagate(bind_res);
            _virq_map[virq] = IrqResolveResult{
                .domain = domain_id,
                .hw_irq = hw_irq,
            };
            return virq;
        }

        /**
         * @brief 按域 ID 获取域.
         */
        [[nodiscard]]
        Result<IrqDomain &> get_domain(domain_t identifier) const {
            auto it = _domains.find(identifier);
            if (it == _domains.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return std::ref(*it->second);
        }

        /**
         * @brief 通过 virq 解析所属中断域.
         */
        [[nodiscard]]
        Result<IrqDomain &> find_domain(virq_t virq) const {
            auto resolve_res = resolve(virq);
            propagate(resolve_res);
            return get_domain(resolve_res.value().domain);
        }

        /**
         * @brief 解析 virq 到 domain 与 hw_irq.
         */
        [[nodiscard]]
        Result<IrqResolveResult> resolve(virq_t virq) const {
            auto it = _virq_map.find(virq);
            if (it == _virq_map.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return it->second;
        }

        /**
         * @brief 使能全局 virq.
         */
        [[nodiscard]]
        Result<void> enable_irq(virq_t virq) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().enable(resolved.value().hw_irq);
        }

        /**
         * @brief 屏蔽全局 virq.
         */
        [[nodiscard]]
        Result<void> disable_irq(virq_t virq) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().disable(resolved.value().hw_irq);
        }

        /**
         * @brief 应答全局 virq.
         */
        [[nodiscard]]
        Result<void> ack_irq(virq_t virq) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().ack(resolved.value().hw_irq);
        }

        /**
         * @brief 设置全局 virq 优先级.
         */
        [[nodiscard]]
        Result<void> set_priority(virq_t virq, irq_prio_t prio) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().set_priority(resolved.value().hw_irq,
                                                     prio);
        }

        /**
         * @brief 设置全局 virq 亲和性.
         */
        [[nodiscard]]
        Result<void> set_affinity(virq_t virq, cpu_mask_t mask) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().set_affinity(resolved.value().hw_irq,
                                                     mask);
        }

        /**
         * @brief 设置全局 virq 的触发方式.
         */
        [[nodiscard]]
        Result<void> set_trigger(virq_t virq, IrqTrigger trigger) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().set_trigger(resolved.value().hw_irq,
                                                    trigger);
        }

        /**
         * @brief 为指定 virq 注册处理函数.
         *
         * @param virq 目标 virq.
         * @param handler 处理函数.
         * @return Result<void> 注册结果.
         */
        Result<void> register_handler(virq_t virq, IrqHandler handler) {
            auto resolve_res = resolve(virq);
            propagate(resolve_res);
            if (_handlers.contains(virq)) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
            _handlers[virq] = std::move(handler);
            void_return();
        }

        /**
         * @brief 注销指定 virq 的处理函数.
         *
         * @param virq 目标 virq.
         * @return Result<void> 注销结果.
         */
        Result<void> unregister_handler(virq_t virq) {
            if (!_handlers.contains(virq)) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            _handlers.erase(virq);
            void_return();
        }

        /**
         * @brief 分发一次 virq 到对应处理函数.
         *
         * @param event 待分发中断事件.
         * @return Result<void> 分发结果.
         */
        Result<void> dispatch(const IrqEvent &event) {
            auto resolve_res = resolve(event.virq);
            propagate(resolve_res);

            auto ack_res = ack_irq(event.virq);
            if (!ack_res.has_value() &&
                ack_res.error() != ErrCode::NOT_SUPPORTED)
            {
                propagate_return(ack_res);
            }

            auto it = _handlers.find(event.virq);
            if (it == _handlers.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }

            it->second(event);
            void_return();
        }
    };

    /**
     * @brief RISC-V CPU 本地中断控制器后端.
     */
    class RiscVIntC : public IrqChip {
    public:
        static constexpr hwirq_t SOFTWARE_LOCAL_IRQ = 3;
        static constexpr hwirq_t CLOCK_LOCAL_IRQ    = 7;
        static constexpr hwirq_t EXTERNAL_LOCAL_IRQ = 9;

        /**
         * @brief 创建一个 CPU 本地中断控制器后端.
         *
         * @param node 统一设备语义节点.
         * @param name 控制器名称.
         * @param identifier 控制器 ID.
         * @param hart_id 该中断控制器所属 hart.
         * @return Result<util::owner<RiscVIntC*>> 构造结果.
         */
        [[nodiscard]]
        static Result<util::owner<RiscVIntC *>> create(
            util::owner<DeviceNode *> node, std::string name,
            intc_t identifier, cpuid_t hart_id) noexcept;

        ~RiscVIntC() override = default;

        [[nodiscard]]
        const char *name() const noexcept override {
            return _name.c_str();
        }

        [[nodiscard]]
        std::vector<PhyArea> mmio_regions() const override;

        [[nodiscard]]
        Result<void> enable_irq(hwirq_t hw_irq) override;

        [[nodiscard]]
        Result<void> disable_irq(hwirq_t hw_irq) override;

        [[nodiscard]]
        Result<void> set_priority(hwirq_t hw_irq, irq_prio_t prio) override;

        [[nodiscard]]
        Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) override;

        [[nodiscard]]
        Result<void> ack_irq(hwirq_t hw_irq) override;

        [[nodiscard]]
        Result<void> set_trigger(hwirq_t hw_irq, IrqTrigger trigger) override;

        /**
         * @brief 获取该控制器所属 hart.
         *
         * @return cpuid_t 所属 hart ID.
         */
        [[nodiscard]]
        cpuid_t hart_id() const noexcept {
            return _hart_id;
        }

    private:
        /**
         * @brief 构造一个 CPU 本地中断控制器后端.
         *
         * @param node 统一设备语义节点.
         * @param name 控制器名称.
         * @param identifier 控制器 ID.
         * @param hart_id 所属 hart.
         */
        RiscVIntC(util::owner<DeviceNode *> node, std::string name,
                  intc_t identifier, cpuid_t hart_id) noexcept
            : _node(std::move(node)),
              _name(std::move(name)),
              _identifier(identifier),
              _hart_id(hart_id) {}

        util::owner<DeviceNode *> _node = util::owner<DeviceNode *>(nullptr);
        std::string _name;
        intc_t _identifier = INVALID_ICTRL_ID;
        cpuid_t _hart_id   = 0;
    };

    /**
     * @brief RISC-V CLINT 控制器实现.
     */
    class Clint : public IrqChip {
    private:
        /**
         * @brief 从统一设备节点读取 MMIO 区域.
         *
         * @return Result<std::vector<PhyArea>> MMIO 区域列表.
         */
        [[nodiscard]]
        static Result<std::vector<PhyArea>> load_mmio_regions(
            const DeviceNode &node) noexcept;

        /**
         * @brief 从统一设备节点读取 CLINT 所需 virq 列表.
         *
         * @return Result<std::pair<virq_t, virq_t>> software/timer virq.
         */
        [[nodiscard]]
        static Result<std::pair<virq_t, virq_t>> load_clint_virqs(
            const DeviceNode &node) noexcept;

        /**
         * @brief 构造一个 CLINT 控制器对象.
         *
         * @param node 统一设备语义节点.
         * @param name 控制器名称.
         * @param identifier 控制器 ID.
         * @param hart_id 默认 hart.
         * @param target_harts 目标 hart 集合.
         */
        Clint(util::owner<DeviceNode *> node, std::string name,
              intc_t identifier, cpuid_t hart_id,
              std::vector<cpuid_t> target_harts) noexcept;

    private:
        util::owner<DeviceNode *> _node = util::owner<DeviceNode *>(nullptr);
        std::string _name;
        intc_t _identifier;
        std::vector<PhyArea> _mmio_regions;
        cpuid_t _hart_id;
        std::vector<cpuid_t> _target_harts;
        virq_t _software_virq = 0;
        virq_t _clock_virq    = 0;

    public:
        /**
         * @brief 由统一设备节点创建 RISC-V CLINT 后端.
         *
         * @param node 统一设备语义节点.
         * @param name 控制器名称.
         * @param identifier 控制器 ID.
         * @param hart_id 默认使用的目标 hart.
         * @param target_harts 该 CLINT 通过 interrupts-extended 解析到的目标
         * hart 集合.
         * @return Result<util::owner<Clint*>> 创建结果.
         */
        [[nodiscard]]
        static Result<util::owner<Clint *>> create(
            util::owner<DeviceNode *> node, std::string name,
            intc_t identifier, cpuid_t hart_id,
            std::vector<cpuid_t> target_harts) noexcept;

        /**
         * @brief 销毁控制器对象.
         */
        ~Clint() override = default;

        /**
         * @brief 获取控制器名称.
         */
        [[nodiscard]]
        const char *name() const noexcept override;
        /**
         * @brief 获取 MMIO 区域列表.
         */
        [[nodiscard]]
        std::vector<PhyArea> mmio_regions() const override;
        /**
         * @brief 使能本地中断.
         */
        [[nodiscard]]
        Result<void> enable_irq(hwirq_t hw_irq) override;
        /**
         * @brief 屏蔽本地中断.
         */
        [[nodiscard]]
        Result<void> disable_irq(hwirq_t hw_irq) override;
        /**
         * @brief 设置中断优先级.
         */
        [[nodiscard]]
        Result<void> set_priority(hwirq_t hw_irq, irq_prio_t prio) override;
        /**
         * @brief 设置中断亲和性.
         */
        [[nodiscard]]
        Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) override;
        /**
         * @brief 应答中断.
         */
        [[nodiscard]]
        Result<void> ack_irq(hwirq_t hw_irq) override;
        /**
         * @brief 设置中断触发方式.
         */
        [[nodiscard]]
        Result<void> set_trigger(hwirq_t hw_irq, IrqTrigger trigger) override;

        /**
         * @brief 获取控制器 ID.
         */
        [[nodiscard]]
        intc_t identifier() const noexcept {
            return _identifier;
        }

        /**
         * @brief 该Clint所属的hart ID.
         */
        [[nodiscard]]
        cpuid_t hart_id() const noexcept {
            return _hart_id;
        }

        /**
         * @brief 获取该 CLINT 后端覆盖的目标 hart 集合.
         *
         * @return const std::vector<cpuid_t>& 目标 hart 列表.
         */
        [[nodiscard]]
        const std::vector<cpuid_t> &target_harts() const noexcept {
            return _target_harts;
        }

        /**
         * @brief 判断指定 hart 是否由该 CLINT 后端覆盖.
         *
         * @param hart_id 待检查 hart.
         * @return true 该 hart 在目标集合中.
         * @return false 该 hart 不在目标集合中.
         */
        [[nodiscard]]
        bool supports_hart(cpuid_t hart_id) const noexcept;

        /**
         * @brief 获取 software interrupt 的 virq.
         *
         * @return virq_t software virq.
         */
        [[nodiscard]]
        virq_t software_virq() const noexcept {
            return _software_virq;
        }

        /**
         * @brief 获取 clock interrupt 的 virq.
         *
         * @return virq_t clock virq.
         */
        [[nodiscard]]
        virq_t clock_virq() const noexcept {
            return _clock_virq;
        }
    };

    /**
     * @brief RISC-V PLIC 上下文描述.
     */
    struct PlicContext {
        cpuid_t hart_id       = 0;
        intc_t parent_intc    = INVALID_ICTRL_ID;
        virq_t external_virq  = 0;
        size_t context_index  = 0;
    };

    /**
     * @brief RISC-V PLIC 控制器实现.
     */
    class Plic final : public IrqChip {
    public:
        /**
         * @brief 构造一个 PLIC 控制器后端.
         *
         * @param node 统一设备语义节点.
         * @param name 控制器名称.
         * @param identifier 控制器 ID.
         * @param source_count 可管理的外部中断源数量.
         * @param contexts PLIC context 与 hart 的映射.
         * @return Result<util::owner<Plic*>> 创建结果.
         */
        [[nodiscard]]
        static Result<util::owner<Plic *>> create(
            util::owner<DeviceNode *> node, std::string name,
            intc_t identifier, hwirq_t source_count,
            std::vector<PlicContext> contexts) noexcept;

        /**
         * @brief 销毁 PLIC 对象.
         */
        ~Plic() override = default;

        /**
         * @brief 获取控制器名称.
         */
        [[nodiscard]]
        const char *name() const noexcept override;

        /**
         * @brief 获取 MMIO 区域列表.
         */
        [[nodiscard]]
        std::vector<PhyArea> mmio_regions() const override;

        /**
         * @brief 使能指定外部中断源.
         */
        [[nodiscard]]
        Result<void> enable_irq(hwirq_t hw_irq) override;

        /**
         * @brief 屏蔽指定外部中断源.
         */
        [[nodiscard]]
        Result<void> disable_irq(hwirq_t hw_irq) override;

        /**
         * @brief 设置指定外部中断源优先级.
         */
        [[nodiscard]]
        Result<void> set_priority(hwirq_t hw_irq, irq_prio_t prio) override;

        /**
         * @brief 设置指定外部中断源亲和性.
         */
        [[nodiscard]]
        Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) override;

        /**
         * @brief 完成当前 hart 上已 claim 的中断源.
         */
        [[nodiscard]]
        Result<void> ack_irq(hwirq_t hw_irq) override;

        /**
         * @brief 设置指定外部中断源触发方式.
         */
        [[nodiscard]]
        Result<void> set_trigger(hwirq_t hw_irq, IrqTrigger trigger) override;

        /**
         * @brief 从当前 hart 所属 context claim 一个待处理外部中断源.
         *
         * @return Result<hwirq_t> claim 得到的外部中断源编号.
         */
        [[nodiscard]]
        Result<hwirq_t> resolve_claim_for_current_hart() noexcept;

        /**
         * @brief 获取控制器 ID.
         *
         * @return intc_t 控制器 ID.
         */
        [[nodiscard]]
        intc_t identifier() const noexcept {
            return _identifier;
        }

        /**
         * @brief 获取可管理的外部中断源数量.
         *
         * @return hwirq_t 外部中断源数量.
         */
        [[nodiscard]]
        hwirq_t source_count() const noexcept {
            return _source_count;
        }

        /**
         * @brief 获取 PLIC context 列表.
         *
         * @return const std::vector<PlicContext>& context 描述列表.
         */
        [[nodiscard]]
        const std::vector<PlicContext> &contexts() const noexcept {
            return _contexts;
        }

    private:
        /**
         * @brief 从统一设备节点读取 MMIO 区域.
         *
         * @return Result<std::vector<PhyArea>> MMIO 区域列表.
         */
        [[nodiscard]]
        static Result<std::vector<PhyArea>> load_mmio_regions(
            const DeviceNode &node) noexcept;

        /**
         * @brief 使用统一设备节点构造 PLIC 对象.
         *
         * @param node 统一设备语义节点.
         * @param name 控制器名称.
         * @param identifier 控制器 ID.
         * @param source_count 中断源数量.
         * @param contexts 上下文列表.
         */
        Plic(util::owner<DeviceNode *> node, std::string name, intc_t identifier,
             hwirq_t source_count, std::vector<PlicContext> contexts) noexcept;

        /**
         * @brief 获取指定 hart 对应的 context.
         *
         * @param hart_id 目标 hart ID.
         * @return Result<const PlicContext&> 对应的 context 引用.
         */
        [[nodiscard]]
        Result<const PlicContext &> context_for_hart(cpuid_t hart_id) const noexcept;

        /**
         * @brief 校验外部中断源编号是否合法.
         *
         * @param hw_irq 待校验的外部中断源编号.
         * @return Result<void> 校验结果.
         */
        [[nodiscard]]
        Result<void> validate_source(hwirq_t hw_irq) const noexcept;

        /**
         * @brief 读取 PLIC 32 位寄存器.
         *
         * @param offset 相对 PLIC 基地址的偏移.
         * @return sus_u32 读取到的寄存器值.
         */
        [[nodiscard]]
        sus_u32 read32(size_t offset) const noexcept;

        /**
         * @brief 向 PLIC 32 位寄存器写入值.
         *
         * @param offset 相对 PLIC 基地址的偏移.
         * @param value 待写入的寄存器值.
         */
        void write32(size_t offset, sus_u32 value) noexcept;

        /**
         * @brief 获取指定 source 的 priority 寄存器偏移.
         *
         * @param hw_irq 外部中断源编号.
         * @return size_t 寄存器偏移.
         */
        [[nodiscard]]
        static constexpr size_t priority_offset(hwirq_t hw_irq) noexcept {
            return static_cast<size_t>(hw_irq) * sizeof(sus_u32);
        }

        /**
         * @brief 获取指定 context 下 source enable bit 的寄存器偏移.
         *
         * @param context_index context 编号.
         * @param hw_irq 外部中断源编号.
         * @return size_t 寄存器偏移.
         */
        [[nodiscard]]
        static constexpr size_t enable_offset(size_t context_index,
                                              hwirq_t hw_irq) noexcept {
            return 0x002000 + context_index * 0x80 +
                   static_cast<size_t>(hw_irq / 32) * sizeof(sus_u32);
        }

        /**
         * @brief 获取指定 context 的 threshold 寄存器偏移.
         *
         * @param context_index context 编号.
         * @return size_t 寄存器偏移.
         */
        [[nodiscard]]
        static constexpr size_t threshold_offset(size_t context_index) noexcept {
            return 0x200000 + context_index * 0x1000;
        }

        /**
         * @brief 获取指定 context 的 claim/complete 寄存器偏移.
         *
         * @param context_index context 编号.
         * @return size_t 寄存器偏移.
         */
        [[nodiscard]]
        static constexpr size_t claim_complete_offset(
            size_t context_index) noexcept {
            return threshold_offset(context_index) + sizeof(sus_u32);
        }

        std::string _name;
        intc_t _identifier = INVALID_ICTRL_ID;
        util::owner<DeviceNode *> _node = util::owner<DeviceNode *>(nullptr);
        std::vector<PhyArea> _mmio_regions;
        hwirq_t _source_count = 0;
        std::vector<PlicContext> _contexts;
        std::unordered_map<cpuid_t, size_t> _context_indices;
        std::unordered_map<cpuid_t, hwirq_t> _claimed_sources;
        volatile sus_u32 *_base = nullptr;
    };

    class ClintAlarm : public Alarm {
    public:
        /**
         * @brief 构造绑定到指定时钟源的 CLINT 定时事件设备.
         *
         * @param clksrc 供定时器换算 tick 的时钟源
         */
        explicit ClintAlarm(ClockSource *clksrc, virq_t clock_virq) noexcept;

        /**
         * @brief 安排下一次定时事件.
         *
         * @param delta 相对当前时刻的触发延迟
         */
        void set_next_event(units::time delta) noexcept override;

        /**
         * @brief 获取该定时器支持的最大触发延迟.
         *
         * @return units::time 最大延迟
         */
        [[nodiscard]]
        units::time max_delta() const noexcept override {
            return UINT64_MAX / _clksrc->frequency();
        }

        /**
         * @brief 设置定时事件到期处理函数.
         *
         * @param handler 到期时调用的回调函数
         */
        void set_handler(Handler &&handler) noexcept override {
            _handler = std::move(handler);
        }

        /**
         * @brief 作为中断系统 handler 处理 clock_virq.
         *
         * @param event 当前 virq 事件.
         */
        void handle_irq(const IrqEvent &event) noexcept;

        /**
         * @brief 获取该定时器绑定的 clock virq.
         *
         * @return virq_t clock virq.
         */
        [[nodiscard]]
        virq_t clock_virq() const noexcept {
            return _clock_virq;
        }

    private:
        Handler _handler;
        units::time _last_recorded_time;
        virq_t _clock_virq = 0;
    };
}  // namespace device
