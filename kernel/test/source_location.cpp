/**
 * @file source_location.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief source_location 测试
 * @version alpha-1.0.0
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/source_location.h>

#include <source_location>

namespace test::source_location {
    constexpr bool constexpr_source_location_smoke() {
        std::source_location empty;
        auto loc = std::source_location::current();
        return empty.line() == 0 && empty.column() == 0 && loc.line() > 0 &&
               loc.file_name() != nullptr && loc.function_name() != nullptr;
    }

    static_assert(constexpr_source_location_smoke());

    static bool contains(const char* text, const char* pattern) noexcept {
        if (text == nullptr || pattern == nullptr) {
            return false;
        }
        if (*pattern == '\0') {
            return true;
        }
        for (const char* cur = text; *cur != '\0'; ++cur) {
            const char* a = cur;
            const char* b = pattern;
            while (*a != '\0' && *b != '\0' && *a == *b) {
                ++a;
                ++b;
            }
            if (*b == '\0') {
                return true;
            }
        }
        return false;
    }

    static std::source_location default_argument_location(
        std::source_location loc = std::source_location::current()) noexcept {
        return loc;
    }

    class FunctionProbe {
    public:
        static std::source_location capture() noexcept {
            return std::source_location::current();
        }
    };

    class CaseDefaultConstruct : public TestCase {
    public:
        CaseDefaultConstruct() : TestCase("默认构造") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::source_location loc;

            ttest(loc.line() == 0);
            ttest(loc.column() == 0);
            ttest(loc.file_name() != nullptr);
            ttest(loc.function_name() != nullptr);
            ttest(loc.file_name()[0] == '\0');
            ttest(loc.function_name()[0] == '\0');
        }
    };

    class CaseCurrent : public TestCase {
    public:
        CaseCurrent() : TestCase("current 捕获") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto loc = std::source_location::current();

            ttest(loc.line() > 0);
            ttest(loc.column() == 0);
            ttest(contains(loc.file_name(), "source_location.cpp"));
            ttest(contains(loc.function_name(), "_run"));
        }
    };

    class CaseFunctionName : public TestCase {
    public:
        CaseFunctionName() : TestCase("函数名捕获") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto loc = FunctionProbe::capture();

            ttest(loc.line() > 0);
            ttest(contains(loc.file_name(), "source_location.cpp"));
            ttest(contains(loc.function_name(), "capture"));
        }
    };

    class CaseDefaultArgument : public TestCase {
    public:
        CaseDefaultArgument() : TestCase("默认实参调用点") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto loc = default_argument_location();

            ttest(loc.line() > 0);
            ttest(contains(loc.file_name(), "source_location.cpp"));
            ttest(contains(loc.function_name(), "_run"));
            ttest(!contains(loc.function_name(), "default_argument_location"));
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseDefaultConstruct());
        cases.push_back(new CaseCurrent());
        cases.push_back(new CaseFunctionName());
        cases.push_back(new CaseDefaultArgument());

        framework.add_category(
            new TestCategory("source_location", std::move(cases)));
    }
}  // namespace test::source_location
