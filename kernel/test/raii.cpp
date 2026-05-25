/**
 * @file raii.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief raii 测试
 * @version alpha-1.0.0
 * @date 2026-05-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/owner.h>
#include <sus/raii.h>
#include <test/raii.h>

#include <utility>

namespace test::raii {
    namespace {
        class Tracked {
        public:
            static int live_count;
            static int destruct_count;

            int value = 0;

            Tracked() {
                ++live_count;
            }

            explicit Tracked(int v) : value(v) {
                ++live_count;
            }

            virtual ~Tracked() {
                --live_count;
                ++destruct_count;
            }

            static void reset_counters() {
                live_count     = 0;
                destruct_count = 0;
            }
        };

        int Tracked::live_count     = 0;
        int Tracked::destruct_count = 0;

        class DerivedTracked : public Tracked {
        public:
            explicit DerivedTracked(int v) : Tracked(v) {}
            ~DerivedTracked() override = default;
        };

        struct CountingDeleter {
            int* delete_count = nullptr;

            void operator()(Tracked* ptr) const noexcept {
                if (delete_count != nullptr) {
                    ++(*delete_count);
                }
                delete ptr;
            }
        };

        class CaseGuard : public TestCase {
        public:
            CaseGuard() : TestCase("Guard 基础行为") {}

            void _run(void* env [[maybe_unused]]) const noexcept override {
                int delete_count = 0;
                {
                    auto guard = util::Guard([&delete_count]() {
                        ++delete_count;
                    });
                }
                ttest(delete_count == 1);

                {
                    auto guard = util::Guard([&delete_count]() {
                        ++delete_count;
                    });
                    guard.release();
                }
                ttest(delete_count == 1);
            }
        };

        class CaseMakeUniqueAndReset : public TestCase {
        public:
            CaseMakeUniqueAndReset() : TestCase("make_unique 与 reset") {}

            void _run(void* env [[maybe_unused]]) const noexcept override {
                Tracked::reset_counters();
                {
                    auto ptr = util::make_unique<Tracked>(7);
                    tassert(static_cast<bool>(ptr));
                    ttest(ptr->value == 7);
                    ttest(Tracked::live_count == 1);

                    ptr.reset(new Tracked(9));
                    ttest(static_cast<bool>(ptr));
                    ttest(ptr->value == 9);
                    ttest(Tracked::live_count == 1);
                    ttest(Tracked::destruct_count == 1);

                    ptr.reset();
                    ttest(!ptr);
                    ttest(Tracked::live_count == 0);
                    ttest(Tracked::destruct_count == 2);
                }
                ttest(Tracked::live_count == 0);
                ttest(Tracked::destruct_count == 2);
            }
        };

        class CaseReleaseAndOwnerBridge : public TestCase {
        public:
            CaseReleaseAndOwnerBridge() : TestCase("release 与 owner 桥接") {}

            void _run(void* env [[maybe_unused]]) const noexcept override {
                Tracked::reset_counters();
                {
                    util::UniquePtr<Tracked> ptr(new Tracked(3));
                    util::owner<Tracked*> raw_owner = ptr.release_owner();
                    ttest(!ptr);
                    ttest(raw_owner.get() != nullptr);
                    ttest(raw_owner->value == 3);
                    ttest(Tracked::live_count == 1);
                    delete raw_owner;
                }
                ttest(Tracked::live_count == 0);
                ttest(Tracked::destruct_count == 1);

                Tracked::reset_counters();
                {
                    util::owner<Tracked*> raw_owner(new Tracked(5));
                    util::UniquePtr<Tracked> ptr(std::move(raw_owner));
                    ttest(!raw_owner);
                    ttest(static_cast<bool>(ptr));
                    ttest(ptr->value == 5);
                }
                ttest(Tracked::live_count == 0);
                ttest(Tracked::destruct_count == 1);
            }
        };

        class CaseMoveAndPolymorphism : public TestCase {
        public:
            CaseMoveAndPolymorphism() : TestCase("移动与跨类型转移") {}

            void _run(void* env [[maybe_unused]]) const noexcept override {
                Tracked::reset_counters();
                {
                    util::UniquePtr<Tracked> dst;
                    auto src = util::make_unique<Tracked>(11);
                    dst      = std::move(src);
                    ttest(!src);
                    ttest(static_cast<bool>(dst));
                    ttest(dst->value == 11);

                    util::UniquePtr<DerivedTracked> derived(
                        new DerivedTracked(13));
                    util::UniquePtr<Tracked> base(std::move(derived));
                    ttest(!derived);
                    ttest(static_cast<bool>(base));
                    ttest(base->value == 13);
                }
                ttest(Tracked::live_count == 0);
                ttest(Tracked::destruct_count == 2);
            }
        };

        class CaseCustomDeleter : public TestCase {
        public:
            CaseCustomDeleter() : TestCase("自定义删除器") {}

            void _run(void* env [[maybe_unused]]) const noexcept override {
                Tracked::reset_counters();
                int delete_count = 0;
                {
                    util::UniquePtr<Tracked, CountingDeleter> ptr(
                        new Tracked(17), CountingDeleter{&delete_count});
                    ttest(static_cast<bool>(ptr));
                    ttest(ptr->value == 17);
                }
                ttest(delete_count == 1);
                ttest(Tracked::live_count == 0);
                ttest(Tracked::destruct_count == 1);
            }
        };
    }  // namespace

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseGuard());
        cases.push_back(new CaseMakeUniqueAndReset());
        cases.push_back(new CaseReleaseAndOwnerBridge());
        cases.push_back(new CaseMoveAndPolymorphism());
        cases.push_back(new CaseCustomDeleter());

        framework.add_category(new TestCategory("raii", std::move(cases)));
    }
}  // namespace test::raii
