#include <balsa/catalog/Catalog.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_set>

namespace {
struct Colour {
    std::string id;
    int value = 0;
};
struct Unnamed {
    std::string name;
};
} // namespace

TEST_CASE("catalog_identified", "[catalog]") {
    STATIC_CHECK(balsa::catalog::Identified<Colour>);
    STATIC_CHECK_FALSE(balsa::catalog::Identified<Unnamed>);
    STATIC_CHECK_FALSE(balsa::catalog::Identified<int>);
}

TEST_CASE("catalog_entries", "[catalog]") {
    balsa::catalog::Catalog<Colour> colours("colour");
    CHECK(colours.all().empty());
    CHECK_FALSE(colours.find("red"));

    const auto red = colours.add({"red", 1});
    const auto blue = colours.add({"blue", 2});
    CHECK(red.id() == "red");
    CHECK(red.value().value == 1);
    CHECK(colours.find("red") == red);
    CHECK(red != blue);
    CHECK(colours.all().size() == 2);

    // Re-adding an id replaces the value behind existing entries.
    const auto again = colours.add({"red", 3});
    CHECK(again == red);
    CHECK(red.value().value == 3);
    CHECK(colours.all().size() == 2);

    // Entries hash by identity.
    const std::unordered_set<balsa::catalog::Entry<Colour>> set{red, blue, again};
    CHECK(set.size() == 2);

    // Moving the catalog keeps entries valid.
    auto moved = std::move(colours);
    CHECK(moved.find("blue") == blue);
    CHECK(blue.value().value == 2);
}
