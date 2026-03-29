/**
 * @file schd_rr.cpp
 * @brief RR 调度器单元测试
 */

#include <schd/rr.h>
#include <test/schd/rr.h>

namespace test::schd::rr {

    struct TestSU : public ::schd::rr::Metadata {
        int id = 0;
    };

    using Scheduler = ::schd::rr::RR<TestSU>;

    class CaseBasicRoundRobin : public TestCase {
    public:
        CaseBasicRoundRobin() : TestCase("RR 基本轮转顺序") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;
            TestSU b;
            TestSU c;
            a.id = 1;
            b.id = 2;
            c.id = 3;

            tassert(schd.insert(&a).has_value(), "插入 a 成功");
            tassert(schd.insert(&b).has_value(), "插入 b 成功");
            tassert(schd.insert(&c).has_value(), "插入 c 成功");

            auto p1 = schd.pick();
            tassert(p1 == &a, "第一次 pick 选择 a");
            tassert(a.state == ThreadState::RUNNING, "a 为 RUNNING");

            auto p2 = schd.pick();
            tassert(p2 == &b, "第二次 pick 选择 b");
            tassert(b.state == ThreadState::RUNNING, "b 为 RUNNING");
            tassert(a.state == ThreadState::READY, "a 被放回就绪队列");

            auto p3 = schd.pick();
            tassert(p3 == &c, "第三次 pick 选择 c");
            tassert(c.state == ThreadState::RUNNING, "c 为 RUNNING");
        }
    };

    class CaseTimeSliceSwitch : public TestCase {
    public:
        CaseTimeSliceSwitch() : TestCase("RR 时间片用尽触发调度") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;
            TestSU b;
            a.id = 1;
            b.id = 2;

            tassert(schd.insert(&a).has_value(), "插入 a 成功");
            tassert(schd.insert(&b).has_value(), "插入 b 成功");

            auto p1 = schd.pick();
            tassert(p1 == &a, "第一次 pick 选择 a");

            // 模拟时间片递增, 直到耗尽
            for (int i = 0; i < Scheduler::TIME_SLICES; ++i) {
                schd.on_tick(units::tick::from_ticks(1));
            }

            tassert(schd.should_switch(), "时间片用尽后应需要切换");

            auto p2 = schd.pick();
            tassert(p2 == &b, "应切换到 b 运行");
            tassert(b.state == ThreadState::RUNNING, "b 为 RUNNING");
            tassert(a.state == ThreadState::READY, "a 回到 READY 队列");
        }
    };

    class CaseYieldAndSwitch : public TestCase {
    public:
        CaseYieldAndSwitch() : TestCase("RR yield 行为") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;
            TestSU b;
            a.id = 1;
            b.id = 2;

            tassert(schd.insert(&a).has_value(), "插入 a 成功");
            tassert(schd.insert(&b).has_value(), "插入 b 成功");

            auto p1 = schd.pick();
            tassert(p1 == &a, "第一次 pick 选择 a");

            schd.yield();
            tassert(a.state == ThreadState::YIELD, "yield 后 a 为 YIELD");
            tassert(schd.should_switch(), "yield 后应需要切换");

            auto p2 = schd.pick();
            tassert(p2 == &b, "应切换到 b");
            tassert(b.state == ThreadState::RUNNING, "b 为 RUNNING");
            tassert(a.state == ThreadState::READY, "a 回到 READY 队列");
        }
    };

    class CaseSuspendAndRemove : public TestCase {
    public:
        CaseSuspendAndRemove() : TestCase("RR suspend/移除 行为") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;
            TestSU b;
            a.id = 1;
            b.id = 2;

            tassert(schd.insert(&a).has_value(), "插入 a 成功");
            tassert(schd.insert(&b).has_value(), "插入 b 成功");

            auto p1 = schd.pick();
            tassert(p1 == &a, "第一次 pick 选择 a");

            schd.suspend();
            tassert(schd.current() == nullptr, "suspend 后 current 为空");
            tassert(a.state == ThreadState::EMPTY, "a 状态为 EMPTY");

            auto rem = schd.remove(&b);
            tassert(rem.has_value(), "从就绪队列移除 b 成功");
            tassert(b.state == ThreadState::EMPTY, "b 状态为 EMPTY");
        }
    };

    class CaseInvalidParams : public TestCase {
    public:
        CaseInvalidParams() : TestCase("RR 参数校验") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;

            auto rnull = schd.insert(nullptr);
            tassert(!rnull.has_value() &&
                        rnull.error() == ErrCode::INVALID_PARAM,
                    "插入空指针应返回 INVALID_PARAM");

            tassert(schd.insert(&a).has_value(), "插入 a 成功");

            TestSU b;
            auto rrm = schd.remove(&b);
            tassert(!rrm.has_value() &&
                        rrm.error() == ErrCode::INVALID_PARAM,
                    "移除未在队列中的 SU 应返回 INVALID_PARAM");
        }
    };

    // 一个简单的“真实调度”模拟: 使用时间片轮转两个任务
    class CaseSimpleSimulation : public TestCase {
    public:
        CaseSimpleSimulation() : TestCase("RR 简单调度模拟") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;
            TestSU b;
            a.id = 1;
            b.id = 2;

            tassert(schd.insert(&a).has_value(), "插入 a 成功");
            tassert(schd.insert(&b).has_value(), "插入 b 成功");

            constexpr int total_ticks = Scheduler::TIME_SLICES * 4;
            int trace[total_ticks]    = {0};

            for (int t = 0; t < total_ticks; ++t) {
                if (schd.current() == nullptr || schd.should_switch()) {
                    auto* next = schd.pick();
                    tassert(next != nullptr, "有就绪任务时 pick 不应返回空");
                }

                auto* cur = schd.current();
                tassert(cur != nullptr, "当前应有正在运行的任务");
                trace[t] = cur->id;

                // 一个 tick 过去
                schd.on_tick(units::tick::from_ticks(1));
            }

            // 预期模式: 前 TIME_SLICES 个 tick 运行 1, 接下来的 TIME_SLICES 个 tick 运行 2,
            // 然后再次轮转回 1,2...
            for (int t = 0; t < total_ticks; ++t) {
                int phase   = (t / Scheduler::TIME_SLICES) % 2;
                int expect  = (phase == 0) ? 1 : 2;
                tassert(trace[t] == expect,
                        "RR 调度在每个时间片区间内应保持同一任务运行");
            }
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseBasicRoundRobin());
        cases.push_back(new CaseTimeSliceSwitch());
        cases.push_back(new CaseYieldAndSwitch());
        cases.push_back(new CaseSuspendAndRemove());
        cases.push_back(new CaseInvalidParams());
        cases.push_back(new CaseSimpleSimulation());

        framework.add_category(new TestCategory("schd_rr", std::move(cases)));
    }

}  // namespace test::schd::rr
