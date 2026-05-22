/**
 * @file ranges.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ranges 测试
 * @version alpha-1.0.0
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/ranges.h>

#include <array>
#include <initializer_list>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace test::ranges {
    class CaseAccessArrays : public TestCase {
    public:
        CaseAccessArrays() : TestCase("数组访问") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            int values[] = {1, 2, 3};

            ttest(std::ranges::begin(values) == values);
            ttest(std::ranges::end(values) == values + 3);
            ttest(std::ranges::size(values) == 3);
            ttest(!std::ranges::empty(values));
            ttest(std::ranges::data(values) == values);
            ttest(*std::ranges::cbegin(values) == 1);

            ttest((std::is_same_v<std::ranges::iterator_t<decltype(values)>,
                                  int*>));
            ttest((std::is_same_v<std::ranges::range_value_t<decltype(values)>,
                                  int>));
            ttest((std::is_same_v<
                   std::ranges::range_reference_t<decltype(values)>, int&>));
        }
    };

    class CaseAccessContainers : public TestCase {
    public:
        CaseAccessContainers() : TestCase("容器访问") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::array<int, 3> arr{4, 5, 6};
            std::vector<int> vec;
            ttest(vec.push_back_nt(7).has_value());
            ttest(vec.push_back_nt(8).has_value());
            std::span<int> sp(arr.data(), arr.size());
            std::string_view sv("abc", 3);

            ttest(std::ranges::size(arr) == 3);
            ttest(*std::ranges::begin(arr) == 4);
            ttest(std::ranges::data(arr) == arr.data());

            ttest(std::ranges::size(vec) == 2);
            ttest(*std::ranges::begin(vec) == 7);
            ttest(std::ranges::data(vec) == vec.data());

            ttest(std::ranges::size(sp) == 3);
            ttest(std::ranges::data(sp) == arr.data());

            ttest(std::ranges::size(sv) == 3);
            ttest(*std::ranges::begin(sv) == 'a');
            ttest(std::ranges::data(sv) == sv.data());
        }
    };

    class CaseConcepts : public TestCase {
    public:
        CaseConcepts() : TestCase("概念与类型萃取") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::initializer_list<int> init{1, 2, 3};

            ttest((std::ranges::range<decltype(init)>));
            ttest((std::ranges::input_range<decltype(init)>));
            ttest((std::is_same_v<
                   std::ranges::range_value_t<decltype(init)>, int>));
            ttest((std::is_same_v<
                   std::ranges::range_reference_t<decltype(init)>,
                   const int&>));
        }
    };

    class CaseViews : public TestCase {
    public:
        CaseViews() : TestCase("views") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            int values[] = {1, 2, 3, 4};

            auto all = std::views::all(values);
            ttest(*all.begin() == 1);
            values[0] = 9;
            ttest(*all.begin() == 9);
            ttest(all.size() == 4);

            auto alias = std::view::all(values);
            ttest(*alias.begin() == 9);

            auto counted = std::views::counted(values + 1, 2);
            ttest(!counted.empty());
            ttest(*counted.begin() == 2);
            ttest(counted.end() == values + 3);
        }
    };

    class CaseFind : public TestCase {
    public:
        CaseFind() : TestCase("find") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            int values[] = {1, 2, 3, 4};
            ttest(std::ranges::find(values, 3) == values + 2);
            ttest(std::ranges::find(values, 8) == values + 4);
            ttest(std::ranges::find(values + 1, values + 4, 2) == values + 1);

            std::vector<std::string> names;
            names.emplace_back("uart");
            names.emplace_back("virtio");
            names.emplace_back("plic");
            ttest(std::ranges::find(names, std::string("virtio")) ==
                  names.begin() + 1);
            ttest(std::ranges::find(names, std::string("missing")) ==
                  names.end());
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseAccessArrays());
        cases.push_back(new CaseAccessContainers());
        cases.push_back(new CaseConcepts());
        cases.push_back(new CaseViews());
        cases.push_back(new CaseFind());

        framework.add_category(new TestCategory("ranges", std::move(cases)));
    }
}  // namespace test::ranges
