#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <ranges>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <string>

TEST_CASE("ranges iota", "[ranges]") {

    int index = 0;
    for (auto &&v : std::views::iota(0, 5)) {
        CHECK(v == index++);
    }
    index = 10;
    for (auto &&v : std::views::iota(10) | std::views::take(10)) {
        CHECK(v == index++);
    }
}

TEST_CASE("ranges zip", "[ranges]") {

    auto range = std::views::iota(0, 10) | std::ranges::to<std::vector<int>>();


    for (auto &&[a, b] : std::views::zip(std::views::iota(0), range)) {
        CHECK(a == b);
        b = 2 * a;
    }

    for (auto [a, b] : std::views::zip(std::views::iota(0), range)) {
        CHECK(2 * a == b);
    }
}


TEST_CASE("ranges enumerate", "[ranges]") {
    auto range = std::views::iota(0, 10) | std::ranges::to<std::vector<int>>();

    for (auto [a, b] : std::views::enumerate(range)) {
        CHECK(int(a) == b);
    }
}


TEST_CASE("ranges heterogeneous line", "[ranges]") {

    std::string line = "C 4 5 6.2";

    auto toks = line | std::views::split(' ');
    auto front = toks.front() | std::ranges::to<std::string>();
    auto nums = toks | std::views::drop(1) | std::views::transform([](const auto &v) {
                    double value;
                    auto s = v | std::ranges::to<std::string>();
                    std::from_chars(s.data(), s.data() + s.size(), value);
                    return value;
                });
    std::cout << "Front: " << front << std::endl;
    for (auto &&num : nums) {
        std::cout << num << std::endl;
    }
    std::array<float, 6> ret;
    auto inp = std::views::concat(nums, std::views::repeat(float(0))) | std::views::take(6);
    std::ranges::copy(inp, ret.begin());
    for (auto &&v : inp) {
        std::cout << v << " ";
    }
    std::cout << std::endl;
}
