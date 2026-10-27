#include <catch2/catch_all.hpp>

#include <balsa/visualization/scene_graph/gltf/Document.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace gltf = balsa::visualization::scene_graph::gltf;
namespace uri = balsa::uri;
using Catch::Matchers::ContainsSubstring;

namespace {

// A scratch directory, removed with its contents on destruction.
class TempDir {
  public:
    TempDir()
      : m_path(std::filesystem::temp_directory_path()
               / std::format("balsa_gltf_test_{}",
                             Catch::getSeed() ^ std::uintptr_t(this))) {
        std::filesystem::create_directories(m_path);
    }
    ~TempDir() { std::filesystem::remove_all(m_path); }
    TempDir(const TempDir &) = delete;
    auto operator=(const TempDir &) -> TempDir & = delete;

    auto path(std::string_view name) const -> std::filesystem::path {
        return m_path / name;
    }
    // Writes `contents` to `name` (creating directories) and returns its path.
    auto write(std::string_view name, std::string_view contents) const
        -> std::filesystem::path {
        const auto file = path(name);
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary)
            .write(contents.data(), std::streamsize(contents.size()));
        return file;
    }

  private:
    std::filesystem::path m_path;
};

auto as_string(std::span<const std::byte> bytes) -> std::string {
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

void append_u32(std::string &out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

// A GLB container with a JSON chunk and an optional BIN chunk, padded as the
// spec requires. `length` overrides the header's total length.
auto make_glb(std::string json, std::string bin = {},
              std::uint32_t length = 0) -> std::string {
    while (json.size() % 4 != 0) { json += ' '; }
    while (bin.size() % 4 != 0) { bin += '\0'; }
    std::string chunks;
    append_u32(chunks, std::uint32_t(json.size()));
    append_u32(chunks, 0x4E4F534A); // "JSON"
    chunks += json;
    if (!bin.empty()) {
        append_u32(chunks, std::uint32_t(bin.size()));
        append_u32(chunks, 0x004E4942); // "BIN\0"
        chunks += bin;
    }
    std::string glb;
    append_u32(glb, 0x46546C67); // "glTF"
    append_u32(glb, 2);
    append_u32(glb, length != 0 ? length : std::uint32_t(12 + chunks.size()));
    return glb + chunks;
}

// A .gltf with one 8-byte buffer and one image, both at `uri`.
auto buffer_and_image(std::string_view uri) -> std::string {
    return std::format(R"({{"asset":{{"version":"2.0"}},)"
                       R"("buffers":[{{"byteLength":8,"uri":"{0}"}}],)"
                       R"("images":[{{"uri":"{0}"}}]}})",
                       uri);
}

} // namespace

TEST_CASE("gltf_glb_container", "[gltf][glb]") {
    TempDir dir;
    const std::string json =
        R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":8}],)"
        R"("bufferViews":[{"buffer":0,"byteOffset":2,"byteLength":4}],)"
        R"("images":[{"bufferView":0,"mimeType":"image/png"}]})";

    SECTION("binary chunk backs buffer 0; bufferView images are views") {
        const auto doc = gltf::load(dir.write("a.glb", make_glb(json, "ab1234cd")));
        REQUIRE(doc);
        CHECK(as_string(doc->buffer(0)) == "ab1234cd");
        CHECK(as_string(doc->buffer_view(0)) == "1234");
        const auto image = doc->image(0);
        CHECK(as_string(image) == "1234");
        CHECK(image.data() == doc->buffer(0).data() + 2); // no copy
    }

    SECTION("malformed containers") {
        const auto error = [&](std::string glb) {
            const auto doc = gltf::load(dir.write("bad.glb", glb));
            REQUIRE_FALSE(doc);
            CHECK(doc.error().code == gltf::LoadError::Code::InvalidGlb);
            return doc.error().message;
        };
        const auto valid = make_glb(json, "ab1234cd");
        CHECK_THAT(error(valid.substr(0, 10)),
                   ContainsSubstring("truncated GLB header"));
        CHECK_THAT(error(make_glb(json, "ab1234cd", 8)),
                   ContainsSubstring("shorter than its header"));
        CHECK_THAT(error(make_glb(json, "ab1234cd", 9999)),
                   ContainsSubstring("exceeds the file size"));
        CHECK_THAT(error(valid.substr(0, 40)),
                   ContainsSubstring("exceeds the file size"));
    }
}

TEST_CASE("gltf_uri_references", "[gltf][uri]") {
    TempDir dir;
    dir.write("model/a.bin", "12345678");
    dir.write("shared dir/b.bin", "abcdefgh");
    const auto load = [&](std::string_view uri,
                          const gltf::LoadOptions &options = {}) {
        return gltf::load(dir.write("model/t.gltf", buffer_and_image(uri)),
                          options);
    };

    SECTION("accepted references") {
        for (std::string_view uri :
             {"a.bin", "./a.bin", "a.bin?v=2#x",
              "data:application/octet-stream;base64,MTIzNDU2Nzg=",
              "DATA:application/octet-stream;base64,MTIzNDU2Nzg="}) {
            CAPTURE(uri);
            const auto doc = load(uri);
            REQUIRE(doc);
            CHECK(as_string(doc->buffer(0)) == "12345678");
            CHECK(as_string(doc->image(0)) == "12345678");
        }
        const auto up = load("../shared%20dir/b.bin");
        REQUIRE(up);
        CHECK(as_string(up->buffer(0)) == "abcdefgh");
    }

    SECTION("rejected references") {
        const auto message = [&](std::string_view uri) {
            const auto doc = load(uri);
            REQUIRE_FALSE(doc);
            return doc.error().message;
        };
        const auto absolute = dir.path("model/a.bin").generic_string();
        CHECK_THAT(message(absolute), ContainsSubstring("not relative"));
        CHECK_THAT(message("//host/a.bin"), ContainsSubstring("not relative"));
        CHECK_THAT(message("\\\\abs\\\\a.bin"),
                   ContainsSubstring("not relative"));
        CHECK_THAT(message("file://" + absolute),
                   ContainsSubstring("absolute file URI"));
        CHECK_THAT(message("https://example.com/a.bin"),
                   ContainsSubstring("no handler for the 'https' scheme"));
        CHECK_THAT(message("1x:a.bin"), ContainsSubstring("invalid scheme"));
        CHECK_THAT(message("nope.bin"), ContainsSubstring("cannot open"));
        CHECK_THAT(message("data:;base64,MTIzNDU2Nzg"),
                   ContainsSubstring("invalid base64"));
    }

    SECTION("custom resolver handles extra schemes") {
        const gltf::LoadOptions options{
            .resolver = uri::Resolver::defaults().with(
                "https",
                [](uri::UriView) -> std::expected<uri::Resolver::Bytes,
                                                  uri::UriError> {
                    return uri::Resolver::Bytes(8, std::byte{'x'});
                })};
        const auto doc = load("https://example.com/a.bin", options);
        REQUIRE(doc);
        CHECK(as_string(doc->buffer(0)) == "xxxxxxxx");
    }
}

TEST_CASE("gltf_load_errors", "[gltf]") {
    TempDir dir;
    dir.write("a.bin", "12345678");
    const auto message = [&](std::string_view json) {
        const auto doc = gltf::load(dir.write("e.gltf", json));
        REQUIRE_FALSE(doc);
        return doc.error().message;
    };

    CHECK_THAT(message(R"({"asset":{"version":"2.0"},)"
                       R"("buffers":[{"byteLength":8,"uri":"a.bin"}],)"
                       R"("bufferViews":[{"buffer":0,"byteOffset":4,"byteLength":8}]})"),
               ContainsSubstring("bufferView 0 exceeds buffer 0"));
    CHECK_THAT(message(R"({"asset":{"version":"2.0"},)"
                       R"("images":[{"bufferView":2,"mimeType":"image/png"}]})"),
               ContainsSubstring("refers to missing bufferView 2"));
    CHECK_THAT(message(R"({"asset":{"version":"2.0"},"images":[{}]})"),
               ContainsSubstring("neither uri nor bufferView"));

    // Every problem is reported, each tagged with what it belongs to.
    const auto many = message(
        R"({"asset":{"version":"2.0"},)"
        R"("buffers":[{"byteLength":8,"uri":"nope1.bin"},)"
        R"({"byteLength":99,"uri":"a.bin"},{"byteLength":4}],)"
        R"("bufferViews":[{"buffer":7,"byteLength":1}],)"
        R"("images":[{"uri":"nope2.png"},{}]})");
    CHECK_THAT(many, ContainsSubstring("6 errors"));
    CHECK_THAT(many, ContainsSubstring("buffer 0: cannot open"));
    CHECK_THAT(many, ContainsSubstring("buffer 1 has 8 bytes, but byteLength is 99"));
    CHECK_THAT(many, ContainsSubstring("buffer 2 has no uri"));
    CHECK_THAT(many, ContainsSubstring("bufferView 0 refers to missing buffer 7"));
    CHECK_THAT(many, ContainsSubstring("image 0: cannot open"));
    CHECK_THAT(many, ContainsSubstring("image 1 has neither uri nor bufferView"));
}

TEST_CASE("gltf_load_manifest", "[gltf]") {
    TempDir dir;
    // Nothing is fetched, so a missing buffer file does not matter.
    const auto path = dir.write(
        "m.gltf", R"({"asset":{"version":"2.0"},)"
                  R"("buffers":[{"byteLength":8,"uri":"nope.bin"}]})");
    const auto manifest = gltf::load_manifest(path);
    REQUIRE(manifest);
    CHECK(manifest->buffers.size() == 1);
    CHECK_FALSE(gltf::load(path));

    const auto glb_manifest = gltf::load_manifest(dir.write(
        "m.glb",
        make_glb(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":4}]})",
                 "abcd")));
    REQUIRE(glb_manifest);
    CHECK(glb_manifest->buffers.size() == 1);
}

TEST_CASE("gltf_versions_and_extensions", "[gltf]") {
    TempDir dir;
    const auto load = [&](std::string_view asset, std::string_view rest = "") {
        return gltf::load_manifest(dir.write(
            "v.gltf", std::format(R"({{"asset":{{{}}}{}}})", asset, rest)));
    };
    for (std::string_view ok : {R"("version":"2.0")", R"("version":"2.1")",
                                R"("version":"2.0","minVersion":"2.0")"}) {
        CAPTURE(ok);
        CHECK(load(ok));
    }
    for (std::string_view unsupported :
         {R"("version":"3.0")", R"("version":"2.1","minVersion":"2.1")"}) {
        CAPTURE(unsupported);
        const auto m = load(unsupported);
        REQUIRE_FALSE(m);
        CHECK(m.error().code == gltf::LoadError::Code::UnsupportedVersion);
    }
    for (std::string_view malformed :
         {R"("version":"2")", R"("version":".0")", R"("version":"2.")",
          R"("version":"2.x")", R"("version":"-2.0")",
          R"("version":"99999999999.0")"}) {
        CAPTURE(malformed);
        const auto m = load(malformed);
        REQUIRE_FALSE(m);
        CHECK(m.error().code == gltf::LoadError::Code::InvalidGltf);
    }

    const auto ext = load(R"("version":"2.0")",
                          R"(,"extensionsUsed":["KHR_a","EXT_b"],)"
                          R"("extensionsRequired":["KHR_a","EXT_b"])");
    REQUIRE_FALSE(ext);
    CHECK(ext.error().code == gltf::LoadError::Code::UnsupportedExtension);
    CHECK_THAT(ext.error().message, ContainsSubstring("KHR_a, EXT_b"));
}
