#define CATCH_CONFIG_MAIN
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/take_exactly.hpp>
#include <catch2/catch_all.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/common.hpp>
#include <charconv>
#include <range/v3/view/all.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/range/conversion.hpp>
#include <iostream>
TEST_CASE("ranges iota", "[ranges]") {

    int index = 0;
    for (auto &&v : ranges::view::iota(0, 5)) {
        CHECK(v == index++);
    }
    index = 10;
    for (auto &&v : ranges::view::iota(10) | ranges::view::take(10)) {
        CHECK(v == index++);
    }
}

TEST_CASE("ranges zip", "[ranges]") {

    auto range = ranges::views::iota(0, 10) | ranges::to<std::vector<int>>();


    for (auto &&[a, b] : ranges::views::zip(ranges::views::iota(0), range)) {
        CHECK(a == b);
        b = 2 * a;
    }

    for (auto [a, b] : ranges::views::zip(ranges::views::iota(0), range)) {
        CHECK(2 * a == b);
    }
}


TEST_CASE("ranges enumerate", "[ranges]") {
    auto range = ranges::views::iota(0, 10) | ranges::to<std::vector<int>>();

    for (auto [a, b] : ranges::views::enumerate(range)) {
        CHECK(int(a) == b);
    }
}


TEST_CASE("ranges heterogeneous line", "[ranges]") {

    std::string line = "C 4 5 6.2";

    auto toks = line | ranges::views::split(' ');
    auto front = toks.front();
    auto nums = toks | ranges::views::drop(1) | ranges::views::transform([](const auto &v) {
                    double value;
                    auto s = v | ranges::to<std::string>;
                    std::from_chars(s.data(), s.data() + s.size(), value);
                    return value;
                });
    std::cout << "Front: " << front << std::endl;
    for (auto &&num : nums) {
        std::cout << num << std::endl;
    }
    std::array<float, 6> ret;
    auto inp = ranges::views::concat(nums, ranges::views::repeat(float(0))) | ranges::view::take_exactly(6);
    std::cout << inp << std::endl;
    ranges::copy(inp, ret.begin());
    std::cout << ranges::views::all(inp) << std::endl;
}
