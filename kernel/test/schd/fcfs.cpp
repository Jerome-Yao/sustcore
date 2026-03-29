/**
 * @file schd_fcfs.cpp
 * @brief FCFS 调度器单元测试
 */

#include <schd/fcfs.h>
#include <test/schd/fcfs.h>

namespace test::schd::fcfs {
    struct TestSU : public ::schd::fcfs::Metadata {
        int id = 0;
    };

    using Scheduler = ::schd::fcfs::FCFS<TestSU>;

    class CaseBasicInsertPick : public TestCase {
    public:
        CaseBasicInsertPick() : TestCase("FCFS 基本插入与选择顺序") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;
            TestSU b;
            TestSU c;
            a.id = 1;
            b.id = 2;
            c.id = 3;

            auto r1 = schd.insert(&a);
            tassert(r1.has_value(), "插入 a 应成功");
            tassert(a.state == ThreadState::READY, "a 状态应为 READY");

            auto r2 = schd.insert(&b);
            tassert(r2.has_value(), "插入 b 应成功");
            tassert(b.state == ThreadState::READY, "b 状态应为 READY");

            auto r3 = schd.insert(&c);
            tassert(r3.has_value(), "插入 c 应成功");
            tassert(c.state == ThreadState::READY, "c 状态应为 READY");

            tassert(schd.current() == nullptr, "初始 current 应为空");

            auto peer1 = schd.peer();
            tassert(peer1 == &a, "peer 应返回最早插入的 a");

            auto p1 = schd.pick();
            tassert(p1 == &a, "第一次 pick 应选择 a");
            tassert(a.state == ThreadState::RUNNING, "a 状态应为 RUNNING");
            tassert(b.state == ThreadState::READY, "b 仍为 READY");
            tassert(c.state == ThreadState::READY, "c 仍为 READY");

            auto peer2 = schd.peer();
            tassert(peer2 == &b, "此时 peer 应返回 b");

            auto p2 = schd.pick();
            tassert(p2 == &b, "第二次 pick 应选择 b");
            tassert(a.state == ThreadState::READY,
                    "a 应被重新放回就绪队列并为 READY");
            tassert(b.state == ThreadState::RUNNING, "b 状态应为 RUNNING");

            auto p3 = schd.pick();
            tassert(p3 == &c, "第三次 pick 应选择 c");
            tassert(c.state == ThreadState::RUNNING, "c 状态应为 RUNNING");
            tassert(a.state == ThreadState::READY, "a 仍为 READY");
            tassert(b.state == ThreadState::READY, "b 应被重新放回就绪队列");
        }
    };

    class CaseYieldAndSwitch : public TestCase {
    public:
        CaseYieldAndSwitch() : TestCase("FCFS yield/should_switch 行为") {}

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
            tassert(!schd.should_switch(), "当前为 RUNNING 时不应切换");

            schd.yield();
            tassert(a.state == ThreadState::YIELD, "yield 后 a 状态应为 YIELD");
            tassert(schd.should_switch(), "yield 后应需要切换");

            auto p2 = schd.pick();
            tassert(p2 == &b, "第二次 pick 应切到 b");
            tassert(b.state == ThreadState::RUNNING, "b 为 RUNNING");
            tassert(a.state == ThreadState::READY,
                    "被重新放回队列的 a 应为 READY");
        }
    };

    class CaseSuspend : public TestCase {
    public:
        CaseSuspend() : TestCase("FCFS suspend 将当前 SU 移出调度器") {}

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
            tassert(schd.current() == nullptr, "suspend 后 current 应为空");
            tassert(a.state == ThreadState::EMPTY,
                    "被 suspend 的 a 状态应为 EMPTY");

            auto p2 = schd.pick();
            tassert(p2 == &b, "之后 pick 应选中 b");
            tassert(b.state == ThreadState::RUNNING, "b 状态应为 RUNNING");
        }
    };

    class CaseInvalidParams : public TestCase {
    public:
        CaseInvalidParams() : TestCase("FCFS 参数校验") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;

            auto rnull = schd.insert(nullptr);
            tassert(!rnull.has_value() && rnull.error() == ErrCode::INVALID_PARAM,
                    "插入空指针应返回 INVALID_PARAM");

            auto r1 = schd.insert(&a);
            tassert(r1.has_value(), "插入 a 成功");

            TestSU b;
            auto rrm = schd.remove(&b);
            tassert(!rrm.has_value() && rrm.error() == ErrCode::INVALID_PARAM,
                    "移除未在队列中的 SU 应返回 INVALID_PARAM");
        }
    };

    // 一个简单的“真实调度”模拟: 两个任务分别运行 2 和 3 个 time slice
    class CaseSimpleSimulation : public TestCase {
    public:
        CaseSimpleSimulation() : TestCase("FCFS 简单调度模拟") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Scheduler schd;
            TestSU a;
            TestSU b;
            a.id = 1;
            b.id = 2;

            // 任务 A 需要运行 2 次, B 需要运行 3 次
            int remain_a = 2;
            int remain_b = 3;

            tassert(schd.insert(&a).has_value(), "插入 a 成功");
            tassert(schd.insert(&b).has_value(), "插入 b 成功");

            int trace[8] = {0};
            int idx      = 0;

            auto has_work = [&]() {
                return remain_a > 0 || remain_b > 0;
            };

            while (has_work()) {
                if (schd.current() == nullptr || schd.should_switch()) {
                    auto* next = schd.pick();
                    tassert(next != nullptr, "有剩余任务时 pick 不应返回空");
                }
                auto* cur = schd.current();
                tassert(cur != nullptr, "当前应有正在运行的任务");

                if (idx < static_cast<int>(sizeof(trace) / sizeof(trace[0]))) {
                    trace[idx++] = cur->id;
                }

                // 执行一个 time slice
                if (cur->id == 1) {
                    --remain_a;
                    if (remain_a == 0) {
                        schd.suspend();
                    } else {
                        schd.yield();
                    }
                } else if (cur->id == 2) {
                    --remain_b;
                    if (remain_b == 0) {
                        schd.suspend();
                    } else {
                        schd.yield();
                    }
                }
            }

            tassert(a.state == ThreadState::EMPTY &&
                        b.state == ThreadState::EMPTY,
                    "所有任务完成后应为 EMPTY 状态");

            // 期望的调度顺序: A, B, A, B, B
            int expect_trace[] = {1, 2, 1, 2, 2};
            for (int i = 0; i < 5; ++i) {
                tassert(trace[i] == expect_trace[i], "FCFS 调度顺序应符合预期");
            }
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseBasicInsertPick());
        cases.push_back(new CaseYieldAndSwitch());
        cases.push_back(new CaseSuspend());
        cases.push_back(new CaseInvalidParams());
        cases.push_back(new CaseSimpleSimulation());

        framework.add_category(
            new TestCategory("schd_fcfs", std::move(cases)));
    }

}  // namespace test::schd_fcfs
