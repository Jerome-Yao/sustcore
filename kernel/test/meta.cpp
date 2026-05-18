/**
 * @file meta.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief meta 测试
 * @version alpha-1.0.0
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/meta.h>

#include <array>
#include <meta>
#include <span>
#include <string_view>
#include <sus/list.h>

namespace test::meta {
    enum class MetaEnum {
        first,
        second,
    };

    struct MetaClass {
        int value;
    };

    consteval std::size_t U_count_enumerators() {
        std::size_t count = 0;
        template for (constexpr auto e : std::meta::enumerators_of(^^MetaEnum)) {
            static_assert(std::meta::is_enumerator(e));
            ++count;
        }
        return count;
    }

    consteval bool U_access_context_smoke() {
        auto unchecked = std::meta::access_context::unchecked();
        auto via_class = unchecked.via(^^MetaClass);
        return via_class.designating_class() == ^^MetaClass;
    }

    static_assert(std::meta::is_type(^^MetaEnum));
    static_assert(std::meta::is_enum_type(^^MetaEnum));
    static_assert(std::meta::is_type(^^MetaClass));
    static_assert(std::meta::is_class_type(^^MetaClass));
    static_assert(std::meta::identifier_of(^^MetaEnum) == "MetaEnum");
    static_assert(U_count_enumerators() == 2);
    static_assert(U_access_context_smoke());

    class CaseBasicQueries : public TestCase {
    public:
        CaseBasicQueries() : TestCase("基础查询") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            ttest(std::meta::identifier_of(^^MetaEnum) == "MetaEnum");
            ttest(std::meta::is_enum_type(^^MetaEnum));
            ttest(std::meta::is_class_type(^^MetaClass));
            ttest(U_count_enumerators() == 2);
        }
    };

    class CaseDefineStatic : public TestCase {
    public:
        CaseDefineStatic() : TestCase("静态对象提升") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            constexpr auto str =
                std::define_static_string(std::string_view("meta", 4));
            constexpr std::array<int, 3> values{1, 2, 3};
            constexpr auto arr = std::define_static_array(values);
            constexpr auto obj = std::define_static_object(MetaClass{42});

            ttest(str[0] == 'm');
            ttest(str[3] == 'a');
            ttest(arr.size() == 3);
            ttest(arr[0] == 1);
            ttest(arr[2] == 3);
            ttest(obj->value == 42);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseBasicQueries());
        cases.push_back(new CaseDefineStatic());

        framework.add_category(new TestCategory("meta", std::move(cases)));
    }
}  // namespace test::meta
