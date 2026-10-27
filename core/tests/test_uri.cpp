#include <catch2/catch_all.hpp>

#include <balsa/uri/Uri.hpp>

#include <optional>
#include <string_view>
#include <utility>

using balsa::uri::Uri;
using balsa::uri::UriError;
using balsa::uri::UriView;

TEST_CASE("uri_parse_components", "[uri]") {
    const auto full = UriView::parse("HTTPS://host:8/a/b%20c?q=1#f");
    REQUIRE(full);
    CHECK(full->scheme() == "HTTPS"); // as written, not normalized
    CHECK(full->authority() == "host:8");
    CHECK(full->path() == "/a/b%20c");
    CHECK(full->query() == "q=1");
    CHECK(full->fragment() == "f");
    CHECK(full->body() == "/a/b%20c?q=1");
    CHECK(full->is_absolute());
    CHECK_FALSE(full->is_relative());

    const auto file = UriView::parse("file:///tmp/x.bin");
    REQUIRE(file);
    CHECK(file->authority() == "");
    CHECK(file->path() == "/tmp/x.bin");
    CHECK_FALSE(file->query());
    CHECK_FALSE(file->fragment());

    const auto data = UriView::parse("data:,x");
    REQUIRE(data);
    CHECK_FALSE(data->authority());
    CHECK(data->body() == ",x");

    const auto rel = UriView::parse("../tex/a%20b.png?v=2#x");
    REQUIRE(rel);
    CHECK_FALSE(rel->scheme());
    CHECK_FALSE(rel->authority());
    CHECK(rel->path() == "../tex/a%20b.png");
    CHECK(rel->query() == "v=2");
    CHECK(rel->fragment() == "x");

    const auto frag_only = UriView::parse("a.bin#x?y");
    REQUIRE(frag_only);
    CHECK_FALSE(frag_only->query()); // '?' inside the fragment
    CHECK(frag_only->fragment() == "x?y");

    const auto empty = UriView::parse("");
    REQUIRE(empty);
    CHECK(empty->path().empty());

    // A ':' after a '/' is part of the path, not a scheme.
    const auto colon = UriView::parse("dir/a:b.bin");
    REQUIRE(colon);
    CHECK_FALSE(colon->scheme());
    CHECK(colon->path() == "dir/a:b.bin");

    CHECK_FALSE(UriView::parse("1http://x"));
    CHECK_FALSE(UriView::parse("a b:x"));
}

TEST_CASE("uri_parse_absolute", "[uri]") {
    CHECK(UriView::parse_absolute("data:,x"));
    CHECK(Uri::parse_absolute("https://example.com"));
    CHECK_FALSE(UriView::parse_absolute("a.bin"));
    CHECK_FALSE(Uri::parse_absolute("/abs/a.bin"));
    REQUIRE_FALSE(UriView::parse_absolute("a.bin"));
    CHECK(UriView::parse_absolute("a.bin").error().code
          == UriError::Code::InvalidUri);
}

TEST_CASE("uri_kinds", "[uri]") {
    const auto kind = [](std::string_view text) {
        const auto ref = UriView::parse(text);
        REQUIRE(ref);
        return std::pair{ref->is_relative(), ref->is_path_relative()};
    };
    CHECK(kind("data:,x") == std::pair{false, false});
    CHECK(kind("C:/x.bin") == std::pair{false, false}); // drive = scheme "C"
    CHECK(kind("a.bin") == std::pair{true, true});
    CHECK(kind("./a:b.bin") == std::pair{true, true});
    CHECK(kind("") == std::pair{true, true});
    CHECK(kind("/abs/a.bin") == std::pair{true, false});
    CHECK(kind("\\abs\\a.bin") == std::pair{true, false});
    CHECK(kind("//host/a.bin") == std::pair{true, false});
}

TEST_CASE("uri_view_and_owner_agree", "[uri]") {
    // Same text, same answers: they differ only in ownership.
    for (std::string_view text :
         {"https://a.example/x?y#z", "../a b.bin?v=1", "data:;base64,AA==",
          "file:///tmp/x", "", "//host/p"}) {
        const auto view = UriView::parse(text);
        const auto owned = Uri::parse(text);
        REQUIRE(view);
        REQUIRE(owned);
        CHECK(owned->text() == view->text());
        CHECK(owned->scheme() == view->scheme());
        CHECK(owned->authority() == view->authority());
        CHECK(owned->path() == view->path());
        CHECK(owned->body() == view->body());
        CHECK(owned->query() == view->query());
        CHECK(owned->fragment() == view->fragment());
        CHECK(owned->is_relative() == view->is_relative());
        CHECK(owned->is_path_relative() == view->is_path_relative());
    }
}

TEST_CASE("uri_resolve", "[uri]") {
    const auto base = Uri::parse("file:///assets/model/scene.gltf");
    REQUIRE(base);

    const auto resolve = [&](std::string_view text) {
        const auto ref = UriView::parse(text);
        REQUIRE(ref);
        return ref->resolve(*base);
    };

    // The base's last segment is replaced, not appended to.
    const auto sibling = resolve("scene.bin");
    REQUIRE(sibling);
    CHECK(sibling->text() == "file:///assets/model/scene.bin");

    const auto up = resolve("../textures/a%20b.png");
    REQUIRE(up);
    CHECK(up->path() == "/assets/textures/a%20b.png");

    // Query and fragment are components, not part of the path.
    const auto q = resolve("tex.png?v=2#x");
    REQUIRE(q);
    CHECK(q->path() == "/assets/model/tex.png");
    CHECK(q->query() == "v=2");
    CHECK(q->fragment() == "x");

    // Absolute references resolve to themselves, normalized.
    const auto data = resolve("DATA:application/octet-stream;base64,AAAA");
    REQUIRE(data);
    CHECK(data->scheme() == "data");

    // The base must be absolute.
    const auto relative_base = Uri::parse("model/scene.gltf");
    REQUIRE(relative_base);
    const auto ref = UriView::parse("a.bin");
    REQUIRE(ref);
    CHECK_FALSE(ref->resolve(*relative_base));
}

TEST_CASE("uri_normalized", "[uri]") {
    const auto uri = Uri::parse("HTTPS://Example.COM/a/./b/../c");
    REQUIRE(uri);
    CHECK(uri->text() == "HTTPS://Example.COM/a/./b/../c"); // kept as written
    const auto normal = uri->normalized();
    REQUIRE(normal);
    CHECK(normal->text() == "https://example.com/a/c");

    const auto localhost = UriView::parse("file://localhost/tmp/x");
    REQUIRE(localhost);
    CHECK(localhost->normalized()->text() == "file:///tmp/x");

    const auto relative = UriView::parse("a.bin");
    REQUIRE(relative);
    CHECK_FALSE(relative->normalized());
}

TEST_CASE("uri_paths", "[uri]") {
    const auto uri = Uri::from_path("/tmp/a b/c#d.bin");
    REQUIRE(uri);
    CHECK(uri->scheme() == "file");
    CHECK_FALSE(uri->fragment());

    const auto path = uri->to_path();
    REQUIRE(path);
    CHECK(*path == std::filesystem::path("/tmp/a b/c#d.bin"));

    const auto to_path = [](std::string_view text) {
        const auto ref = UriView::parse(text);
        REQUIRE(ref);
        return ref->to_path();
    };
    // Normalized before decoding.
    CHECK(to_path("FILE:///tmp/a/../x") == std::filesystem::path("/tmp/x"));
    CHECK(to_path("file://localhost/tmp/x") == std::filesystem::path("/tmp/x"));

    CHECK(to_path("https://example.com/x").error().code
          == UriError::Code::NotAFileUri);
    CHECK(to_path("file://server/share/x").error().code
          == UriError::Code::NotAFileUri);
    CHECK(to_path("relative.bin").error().code == UriError::Code::NotAFileUri);
    CHECK(to_path("file:///tmp/a%00b").error().code
          == UriError::Code::InvalidPath);
}

template <typename T>
concept can_view = requires(T &&t) { std::forward<T>(t).view(); };

TEST_CASE("uri_ownership", "[uri]") {
    const auto uri = Uri::parse("https://example.com/a?b#c");
    REQUIRE(uri);
    const auto view = uri->view();
    CHECK(view.text().data() == uri->text().data()); // borrows, no copy
    CHECK(view.path() == "/a");

    // Uri(UriView) copies.
    constexpr std::string_view text = "../x.bin#f";
    const auto borrowed = UriView::parse(text);
    REQUIRE(borrowed);
    const Uri copied(*borrowed);
    CHECK(copied.text() == text);
    CHECK(copied.text().data() != text.data());
    CHECK(copied.fragment() == "f");

    // Copies and moves own their text.
    auto original = std::optional<Uri>(*uri);
    const auto moved = std::move(*original);
    original.reset();
    CHECK(moved.path() == "/a");
    CHECK(moved.view().path() == "/a");

    // view() is deleted on a temporary Uri. (Not on Uri::parse(...)->view():
    // expected's operator-> yields an lvalue, so that still compiles.)
    static_assert(!can_view<Uri>);
    static_assert(can_view<const Uri &>);
}
