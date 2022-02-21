#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/range/conversion.hpp>
TEST_CASE("ranges iota", "[ranges]") {

    int index = 0;
    for(auto&& v: ranges::view::iota(0,5)) 
    {
        CHECK(v == index++);
    }
    index = 10;
    for(auto&& v: ranges::view::iota(10) | ranges::view::take(10)) 
    {
        CHECK(v == index++);
    }
}

TEST_CASE("ranges zip", "[ranges]") {

    auto range = ranges::views::iota(0,10) | ranges::to<std::vector<int>>();



    for(auto&& [a,b]: ranges::views::zip(ranges::views::iota(0),range))
    {
        CHECK(a == b);
        b = 2 * a;

    }
    
    for(auto [a,b]: ranges::views::zip(ranges::views::iota(0),range))
    {
        CHECK(2 * a == b);

    }
}

    
TEST_CASE("ranges enumerate", "[ranges]") {
    auto range = ranges::views::iota(0,10) | ranges::to<std::vector<int>>();

    for(auto [a,b]: ranges::views::enumerate(range))
    {
        CHECK(int(a) == b);

    }
}
