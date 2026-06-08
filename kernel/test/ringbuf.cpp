/**
 * @file ringbuf.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ringbuf 测试
 * @version alpha-1.0.0
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/ringbuf.h>

#include <nt/errors.h>
#include <sus/ringbuf.h>

namespace test::ringbuf {
    class CaseBasicPushPop : public TestCase {
    public:
        CaseBasicPushPop() : TestCase("基础入队出队") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            util::RingBuffer<int> buf(4);

            ttest(buf.empty());
            ttest(buf.capacity() == 4);
            ttest(buf.size() == 0);

            ttest(buf.push(1).has_value());
            ttest(buf.emplace(2).has_value());
            ttest(buf.push(3).has_value());
            ttest(buf.size() == 3);
            ttest(buf.full());
            ttest(buf.front() == 1);
            ttest(buf.contains(2));

            auto overflow = buf.push(4);
            ttest(!overflow);
            ttest(overflow.error() == std::error_type::OVERFLOW_ERROR);

            ttest(buf.pop().has_value());
            ttest(buf.front() == 2);
            ttest(buf.size() == 2);

            ttest(buf.pop().has_value());
            ttest(buf.pop().has_value());
            ttest(buf.empty());

            auto underflow = buf.pop();
            ttest(!underflow);
            ttest(underflow.error() == std::error_type::UNDERFLOW_ERROR);
        }
    };

    class CaseWrapAround : public TestCase {
    public:
        CaseWrapAround() : TestCase("环绕索引") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            util::RingBuffer<int> buf(5);

            ttest(buf.push(10).has_value());
            ttest(buf.push(20).has_value());
            ttest(buf.push(30).has_value());
            ttest(buf.pop().has_value());
            ttest(buf.pop().has_value());

            ttest(buf.push(40).has_value());
            ttest(buf.push(50).has_value());
            ttest(buf.push(60).has_value());

            ttest(buf.size() == 4);
            ttest(buf.front() == 30);
            ttest(buf.contains(40));
            ttest(buf.contains(60));
            ttest(!buf.contains(10));

            ttest(buf.pop().has_value());
            ttest(buf.front() == 40);
            ttest(buf.pop().has_value());
            ttest(buf.front() == 50);
        }
    };

    class CaseCopyMove : public TestCase {
    public:
        CaseCopyMove() : TestCase("拷贝与移动") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            util::RingBuffer<int> buf(5);
            ttest(buf.push(1).has_value());
            ttest(buf.push(2).has_value());
            ttest(buf.push(3).has_value());
            ttest(buf.pop().has_value());
            ttest(buf.push(4).has_value());

            util::RingBuffer<int> copy(buf);
            ttest(copy.size() == 3);
            ttest(copy.front() == 2);
            ttest(copy.contains(3));
            ttest(copy.contains(4));

            ttest(copy.pop().has_value());
            ttest(copy.front() == 3);
            ttest(buf.front() == 2);

            util::RingBuffer<int> moved(std::move(copy));
            ttest(moved.size() == 2);
            ttest(moved.front() == 3);
            ttest(copy.empty());

            util::RingBuffer<int> assigned(3);
            assigned = buf;
            ttest(assigned.size() == 3);
            ttest(assigned.front() == 2);

            util::RingBuffer<int> move_assigned(3);
            move_assigned = std::move(assigned);
            ttest(move_assigned.size() == 3);
            ttest(move_assigned.front() == 2);
            ttest(assigned.empty());
        }
    };

    class Tracked {
    public:
        static int live_count;
        int value = 0;

        Tracked() {
            ++live_count;
        }
        explicit Tracked(int v) : value(v) {
            ++live_count;
        }
        Tracked(const Tracked& other) : value(other.value) {
            ++live_count;
        }
        Tracked(Tracked&& other) noexcept : value(other.value) {
            other.value = -1;
            ++live_count;
        }
        Tracked& operator=(const Tracked& other) {
            value = other.value;
            return *this;
        }
        Tracked& operator=(Tracked&& other) noexcept {
            value       = other.value;
            other.value = -1;
            return *this;
        }
        ~Tracked() {
            --live_count;
        }

        bool operator==(const Tracked& other) const noexcept {
            return value == other.value;
        }
    };

    int Tracked::live_count = 0;

    class CaseLifetime : public TestCase {
    public:
        CaseLifetime() : TestCase("对象生命周期") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            Tracked::live_count = 0;
            {
                util::RingBuffer<Tracked> buf(4);
                ttest(buf.emplace(1).has_value());
                ttest(buf.emplace(2).has_value());
                ttest(buf.emplace(3).has_value());
                ttest(Tracked::live_count == 3);

                ttest(buf.pop().has_value());
                ttest(Tracked::live_count == 2);

                buf.clear();
                ttest(Tracked::live_count == 0);

                ttest(buf.emplace(4).has_value());
                ttest(Tracked::live_count == 1);
            }
            ttest(Tracked::live_count == 0);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseBasicPushPop());
        cases.push_back(new CaseWrapAround());
        cases.push_back(new CaseCopyMove());
        cases.push_back(new CaseLifetime());

        framework.add_category(new TestCategory("ringbuf", std::move(cases)));
    }
}  // namespace test::ringbuf
