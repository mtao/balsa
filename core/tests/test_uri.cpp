#include <catch2/catch_all.hpp>

#include <balsa/uri/Uri.hpp>

using balsa::uri::Uri;
using balsa::uri::UriError;

TEST_CASE("uri_parse_components", "[uri]") {
    const auto uri = Uri::parse("https://example.com/a/b%20c?x=1#frag");
    REQUIRE(uri);
    CHECK(uri->scheme() == "https");
    CHECK(uri->path() == "/a/b%20c");
    CHECK(uri->query() == "x=1");
    CHECK(uri->fragment() == "frag");
    CHECK(uri->body() == "/a/b%20c?x=1");

    const auto bare = Uri::parse("file:///tmp/x.bin");
    REQUIRE(bare);
    CHECK(bare->scheme() == "file");
    CHECK(bare->path() == "/tmp/x.bin");
    CHECK_FALSE(bare->query());
    CHECK_FALSE(bare->fragment());
    CHECK(bare->body() == "/tmp/x.bin");

    CHECK_FALSE(Uri::parse("not a uri"));
    CHECK_FALSE(Uri::parse("relative/path.bin"));
}

TEST_CASE("uri_resolve_relative", "[uri]") {
    const auto base = Uri::parse("file:///assets/model/scene.gltf");
    REQUIRE(base);

    // The base's last segment is replaced, not appended to.
    const auto sibling = Uri::parse("scene.bin", *base);
    REQUIRE(sibling);
    CHECK(sibling->href() == "file:///assets/model/scene.bin");

    const auto up = Uri::parse("../textures/a%20b.png", *base);
    REQUIRE(up);
    CHECK(up->path() == "/assets/textures/a%20b.png");

    // Query and fragment are components, not part of the path.
    const auto q = Uri::parse("tex.png?v=2#x", *base);
    REQUIRE(q);
    CHECK(q->path() == "/assets/model/tex.png");
    CHECK(q->query() == "v=2");
    CHECK(q->fragment() == "x");

    const auto data = Uri::parse("data:application/octet-stream;base64,AAAA",
                                 *base);
    REQUIRE(data);
    CHECK(data->scheme() == "data");
}

TEST_CASE("uri_paths", "[uri]") {
    const auto uri = Uri::from_path("/tmp/a b/c#d.bin");
    REQUIRE(uri);
    CHECK(uri->scheme() == "file");
    CHECK_FALSE(uri->fragment());

    const auto path = uri->to_path();
    REQUIRE(path);
    CHECK(*path == std::filesystem::path("/tmp/a b/c#d.bin"));

    const auto http = Uri::parse("https://example.com/x");
    REQUIRE(http);
    REQUIRE_FALSE(http->to_path());
    CHECK(http->to_path().error().code == UriError::Code::NotAFileUri);

    const auto remote = Uri::parse("file://server/share/x");
    REQUIRE(remote);
    CHECK_FALSE(remote->to_path());

    const auto localhost = Uri::parse("file://localhost/tmp/x");
    REQUIRE(localhost);
    REQUIRE(localhost->to_path());
    CHECK(*localhost->to_path() == std::filesystem::path("/tmp/x"));

    const auto nul = Uri::parse("file:///tmp/a%00b");
    REQUIRE(nul);
    REQUIRE_FALSE(nul->to_path());
    CHECK(nul->to_path().error().code == UriError::Code::InvalidPath);
}
