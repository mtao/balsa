#include <catch2/catch_all.hpp>

#include <balsa/uri/Resolver.hpp>
#include <balsa/uri/Uri.hpp>

#include <filesystem>
#include <fstream>
#include <string_view>

using balsa::uri::Resolver;
using balsa::uri::Uri;
using balsa::uri::UriError;

namespace {
auto as_bytes(std::string_view text) -> Resolver::bytes_type {
    const auto *begin = reinterpret_cast<const std::byte *>(text.data());
    return {begin, begin + text.size()};
}
} // namespace

TEST_CASE("resolver_defaults", "[uri][resolver]") {
    const auto resolver = Resolver::defaults();
    CHECK(resolver.handles("data"));
    CHECK(resolver.handles("file"));
    CHECK_FALSE(resolver.handles("https"));

    const auto data = Uri::parse("data:;base64,Zm9vYmFy");
    REQUIRE(data);
    CHECK(resolver.fetch(data->view()) == as_bytes("foobar"));

    // Schemes match case-insensitively; references need not be normalized.
    const auto upper = balsa::uri::UriView::parse("DATA:;base64,Zm9vYmFy");
    REQUIRE(upper);
    CHECK(resolver.fetch(*upper) == as_bytes("foobar"));

    const auto relative = balsa::uri::UriView::parse("a.bin");
    REQUIRE(relative);
    CHECK(resolver.fetch(*relative).error().code == UriError::Code::InvalidUri);

    const auto dir = std::filesystem::temp_directory_path() / "balsa resolver";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "a.bin", std::ios::binary) << "hello";
    const auto base = Uri::from_path(dir / "scene.gltf");
    REQUIRE(base);
    const auto file = balsa::uri::UriView::parse("a.bin")->resolve(*base);
    REQUIRE(file);
    CHECK(resolver.fetch(file->view()) == as_bytes("hello"));

    const auto missing = balsa::uri::UriView::parse("missing.bin")->resolve(*base);
    REQUIRE(missing);
    REQUIRE_FALSE(resolver.fetch(missing->view()));
    CHECK(resolver.fetch(missing->view()).error().code == UriError::Code::Io);
    std::filesystem::remove_all(dir);

    const auto https = Uri::parse("https://example.com/a.bin");
    REQUIRE(https);
    REQUIRE_FALSE(resolver.fetch(https->view()));
    CHECK(resolver.fetch(https->view()).error().code
          == UriError::Code::UnsupportedScheme);
}

TEST_CASE("resolver_add", "[uri][resolver]") {
    const auto base = Resolver::defaults();
    auto extended = base;
    extended.add("HTTPS", [](balsa::uri::UriView) -> Resolver::result_type {
        return as_bytes("remote");
    });
    // Copies are independent; schemes match case-insensitively.
    CHECK_FALSE(base.handles("https"));
    CHECK(extended.handles("https"));
    CHECK(extended.handles("data"));

    const auto https = Uri::parse("https://example.com/a.bin");
    REQUIRE(https);
    CHECK(extended.fetch(https->view()) == as_bytes("remote"));

    // Chained on a temporary, replacing a handler.
    const auto no_files = Resolver::defaults().add(
        "file", [](balsa::uri::UriView uri) -> Resolver::result_type {
            return std::unexpected(UriError{UriError::Code::Io,
                                            std::string(uri.text())});
        });
    const auto file = Uri::parse("file:///etc/hostname");
    REQUIRE(file);
    CHECK_FALSE(no_files.fetch(file->view()));
}
