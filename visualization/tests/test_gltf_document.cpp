#include <catch2/catch_all.hpp>

// Viewer headers first: the glTF namespace shadows balsa::scene_graph
// for code inside balsa::visualization.
#include <balsa/scene_graph/SceneLoader.hpp>
#include <balsa/scene_graph/AttributeSemantic.hpp>
#include <balsa/scene_graph/BasicShading.hpp>
#include <balsa/visualization/normalize_mesh.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>
#include <numbers>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/vulkan_mesh_drawable.hpp>
#include <balsa/scene_graph/MeshAttribute.hpp>
#include <balsa/visualization/scene_graph/gltf/Document.hpp>
#include <balsa/visualization/scene_graph/gltf/material_conversion.hpp>
#include <balsa/visualization/scene_graph/gltf/mesh_conversion.hpp>
#include <balsa/visualization/scene_graph/gltf/scene_conversion.hpp>
#include <balsa/scene_graph/MeshInstance.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <variant>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>

namespace gltf = balsa::visualization::gltf;
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

// Little-endian bytes of `value` appended to `out`.
template <typename T>
void append_le(std::string &out, T value) {
    using Bits = std::conditional_t<sizeof(T) == 1, std::uint8_t,
                 std::conditional_t<sizeof(T) == 2, std::uint16_t, std::uint32_t>>;
    const auto bits = std::bit_cast<Bits>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<char>((bits >> (8 * i)) & 0xFF));
    }
}

void append_u32(std::string &out, std::uint32_t value) { append_le(out, value); }

template <typename T>
auto le_bytes(std::initializer_list<T> values) -> std::string {
    std::string out;
    for (const auto v : values) { append_le(out, v); }
    return out;
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
            .resolver = uri::Resolver::defaults().add(
                "https", [](uri::UriView) -> uri::Resolver::result_type {
                    return uri::Resolver::bytes_type(8, std::byte{'x'});
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

namespace {
// Loads a GLB with `json` (asset added) and `bin` as its binary chunk.
auto load_glb(const TempDir &dir, std::string_view json, std::string bin)
    -> std::expected<gltf::Document, gltf::LoadError> {
    return gltf::load(dir.write(
        "acc.glb",
        make_glb(std::format(R"({{"asset":{{"version":"2.0"}},{}}})", json),
                 std::move(bin))));
}

auto to_attribute(const gltf::AccessorView &view)
    -> balsa::scene_graph::MeshAttribute {
    return std::visit(
        [&](const auto &values) {
            return balsa::scene_graph::MeshAttribute(
                "values", balsa::scene_graph::AttributeDomain::Vertex, values,
                view.normalized);
        },
        view.data);
}

template <typename T>
auto span_of(const gltf::AccessorView &view) -> gltf::AccessorSpan<T> {
    REQUIRE(std::holds_alternative<gltf::AccessorSpan<T>>(view.data));
    return std::get<gltf::AccessorSpan<T>>(view.data);
}
} // namespace

TEST_CASE("gltf_accessors", "[gltf][accessor]") {
    TempDir dir;

    SECTION("packed float VEC3") {
        const auto bin = le_bytes<float>({1, 2, 3, 4, 5, 6});
        const auto doc = load_glb(
            dir,
            R"("buffers":[{"byteLength":24}],)"
            R"("bufferViews":[{"buffer":0,"byteLength":24}],)"
            R"("accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"VEC3"}])",
            bin);
        REQUIRE(doc);
        const auto view = doc->accessor(0);
        CHECK(view.type() == gltf::AccessorType::VEC3);
        CHECK(view.rows() == 3);
        CHECK(view.columns() == 1);
        CHECK(view.count() == 2);
        CHECK_FALSE(view.normalized);
        const auto span = span_of<float>(view);
        CHECK(span[0, 0, 0] == 1.f);
        CHECK(span[2, 0, 1] == 6.f);

        // Plugs straight into MeshAttribute as a vector attribute.
        const auto attribute = to_attribute(view);
        CHECK(attribute.is_vector());
        CHECK(attribute.rows() == 3);
        CHECK(attribute.component_count() == 3);
        CHECK(attribute.count() == 2);
        const auto stored = attribute.view<float, 3>();
        REQUIRE(stored);
        CHECK((*stored)[1, 1] == 5.f);
        CHECK((*stored)[2, 1] == 6.f);
    }

    SECTION("interleaved positions and indices") {
        // Two vertices of (VEC2 position, VEC2 uv), byteStride 16, then
        // three uint16 indices.
        auto bin = le_bytes<float>({1, 2, 10, 20, 3, 4, 30, 40});
        bin += le_bytes<std::uint16_t>({0, 1, 1});
        const auto doc = load_glb(
            dir,
            R"("buffers":[{"byteLength":38}],)"
            R"("bufferViews":[{"buffer":0,"byteLength":32,"byteStride":16},)"
            R"({"buffer":0,"byteOffset":32,"byteLength":6}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"componentType":5126,"count":2,"type":"VEC2"},)"
            R"({"bufferView":0,"byteOffset":8,"componentType":5126,"count":2,"type":"VEC2"},)"
            R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}])",
            bin);
        REQUIRE(doc);
        const auto positions = span_of<float>(doc->accessor(0));
        CHECK(positions[0, 0, 1] == 3.f);
        CHECK(positions[1, 0, 1] == 4.f);
        const auto uvs = span_of<float>(doc->accessor(1));
        CHECK(uvs[0, 0, 0] == 10.f);
        CHECK(uvs[1, 0, 1] == 40.f);
        const auto indices = doc->accessor(2);
        CHECK(indices.type() == gltf::AccessorType::SCALAR);
        const auto i = span_of<std::uint16_t>(indices);
        CHECK(i[0, 0, 2] == 1);
    }

    SECTION("normalized colors") {
        const auto doc = load_glb(
            dir,
            R"("buffers":[{"byteLength":4}],)"
            R"("bufferViews":[{"buffer":0,"byteLength":4}],)"
            R"("accessors":[{"bufferView":0,"componentType":5121,"normalized":true,)"
            R"("count":1,"type":"VEC4"}])",
            le_bytes<std::uint8_t>({255, 128, 0, 64}));
        REQUIRE(doc);
        const auto view = doc->accessor(0);
        CHECK(view.normalized);
        CHECK(span_of<std::uint8_t>(view)[0, 0, 0] == 255);
        CHECK(span_of<std::uint8_t>(view)[3, 0, 0] == 64);
    }

    SECTION("byte MAT2 with padded columns") {
        // Each 2-byte column is padded to 4 bytes: [a b _ _ c d _ _].
        const auto doc = load_glb(
            dir,
            R"("buffers":[{"byteLength":8}],)"
            R"("bufferViews":[{"buffer":0,"byteLength":8}],)"
            R"("accessors":[{"bufferView":0,"componentType":5121,"normalized":true,)"
            R"("count":1,"type":"MAT2"}])",
            le_bytes<std::uint8_t>({1, 2, 99, 99, 3, 4, 99, 99}));
        REQUIRE(doc);
        const auto view = doc->accessor(0);
        CHECK(view.type() == gltf::AccessorType::MAT2);
        const auto m = span_of<std::uint8_t>(view);
        CHECK(m[0, 0, 0] == 1);
        CHECK(m[1, 0, 0] == 2);
        CHECK(m[0, 1, 0] == 3); // column 1 starts after the padding
        CHECK(m[1, 1, 0] == 4);

        // A matrix-valued MeshAttribute, without the padding.
        const auto attribute = to_attribute(view);
        CHECK_FALSE(attribute.is_vector());
        CHECK(attribute.rows() == 2);
        CHECK(attribute.columns() == 2);
        CHECK(attribute.component_count() == 4);
        CHECK(attribute.normalized());
        const auto tensor = attribute.tensor_view<std::uint8_t, 2, 2>();
        REQUIRE(tensor);
        CHECK((*tensor)[0, 0, 0] == 1);
        CHECK((*tensor)[1, 0, 0] == 2);
        CHECK((*tensor)[0, 1, 0] == 3);
        CHECK((*tensor)[1, 1, 0] == 4);
    }

    SECTION("sparse over a bufferView and over zeros") {
        // Base: four floats. Sparse: indices [1, 3], values [10, 30].
        auto bin = le_bytes<float>({1, 2, 3, 4});
        bin += le_bytes<std::uint8_t>({1, 3, 0, 0});
        bin += le_bytes<float>({10, 30});
        const auto doc = load_glb(
            dir,
            R"("buffers":[{"byteLength":28}],)"
            R"("bufferViews":[{"buffer":0,"byteLength":16},)"
            R"({"buffer":0,"byteOffset":16,"byteLength":2},)"
            R"({"buffer":0,"byteOffset":20,"byteLength":8}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"componentType":5126,"count":4,"type":"SCALAR",)"
            R"("sparse":{"count":2,"indices":{"bufferView":1,"componentType":5121},)"
            R"("values":{"bufferView":2}}},)"
            R"({"componentType":5126,"count":4,"type":"SCALAR",)"
            R"("sparse":{"count":2,"indices":{"bufferView":1,"componentType":5121},)"
            R"("values":{"bufferView":2}}}])",
            bin);
        REQUIRE(doc);
        const auto over_view = span_of<float>(doc->accessor(0));
        CHECK(over_view[0, 0, 0] == 1.f);
        CHECK(over_view[0, 0, 1] == 10.f);
        CHECK(over_view[0, 0, 2] == 3.f);
        CHECK(over_view[0, 0, 3] == 30.f);
        const auto over_zeros = span_of<float>(doc->accessor(1));
        CHECK(over_zeros[0, 0, 0] == 0.f);
        CHECK(over_zeros[0, 0, 1] == 10.f);
        CHECK(over_zeros[0, 0, 2] == 0.f);
        CHECK(over_zeros[0, 0, 3] == 30.f);
        // The base bufferView is untouched.
        CHECK(span_of<float>(doc->accessor(0)).data_handle()
              != doc->buffer_view(0).data());
    }

    SECTION("sparse index widths") {
        // Base of three zeros; sparse index [2] -> 7, stored as 16 and 32 bit.
        for (const auto &[type, index] :
             {std::pair{5123, le_bytes<std::uint16_t>({2, 0})},
              std::pair{5125, le_bytes<std::uint32_t>({2})}}) {
            CAPTURE(type);
            const auto doc = load_glb(
                dir,
                std::format(
                    R"("buffers":[{{"byteLength":8}}],)"
                    R"("bufferViews":[{{"buffer":0,"byteLength":4}},)"
                    R"({{"buffer":0,"byteOffset":4,"byteLength":4}}],)"
                    R"("accessors":[{{"componentType":5126,"count":3,"type":"SCALAR",)"
                    R"("sparse":{{"count":1,"indices":{{"bufferView":0,"componentType":{}}},)"
                    R"("values":{{"bufferView":1}}}}}}])",
                    type),
                index + le_bytes<float>({7}));
            REQUIRE(doc);
            const auto values = span_of<float>(doc->accessor(0));
            CHECK(values[0, 0, 0] == 0.f);
            CHECK(values[0, 0, 2] == 7.f);
        }
    }

    SECTION("invalid accessors") {
        const auto message = [&](std::string_view accessors,
                                 std::string_view views =
                                     R"({"buffer":0,"byteLength":8})") {
            const auto doc = load_glb(
                dir,
                std::format(R"("buffers":[{{"byteLength":8}}],)"
                            R"("bufferViews":[{}],"accessors":[{}])",
                            views,
                            accessors),
                std::string(8, '\0'));
            REQUIRE_FALSE(doc);
            return doc.error().message;
        };
        CHECK_THAT(message(R"({"bufferView":0,"componentType":5126,"count":3,"type":"SCALAR"})"),
                   ContainsSubstring("needs 12 bytes, but bufferView 0 has 8"));
        CHECK_THAT(message(R"({"bufferView":0,"byteOffset":2,"componentType":5126,"count":1,"type":"SCALAR"})"),
                   ContainsSubstring("not aligned"));
        CHECK_THAT(message(R"({"bufferView":0,"componentType":5126,"count":2,"type":"VEC2"})",
                           R"({"buffer":0,"byteLength":8,"byteStride":4})"),
                   ContainsSubstring("cannot use byteStride 4"));
        CHECK_THAT(message(R"({"bufferView":0,"componentType":5126,"normalized":true,"count":1,"type":"SCALAR"})"),
                   ContainsSubstring("is normalized"));
        CHECK_THAT(message(R"({"bufferView":3,"componentType":5126,"count":1,"type":"SCALAR"})"),
                   ContainsSubstring("missing bufferView 3"));
        CHECK_THAT(message(R"({"componentType":5126,"count":2,"type":"SCALAR",)"
                           R"("sparse":{"count":2,"indices":{"bufferView":0,"componentType":5121},)"
                           R"("values":{"bufferView":0}}})"),
                   ContainsSubstring("strictly increasing"));
    }
}

namespace {
// A GLB with `vertices` float positions (accessor 0), `extra` bytes in
// bufferView 1, and the JSON members in `rest`.
constexpr std::string_view positions_accessor =
    R"({"bufferView":0,"componentType":5126,"type":"VEC3","count":)";

auto load_mesh_glb(const TempDir &dir, std::size_t vertices, std::string extra,
                   std::string_view rest)
    -> std::expected<gltf::Document, gltf::LoadError> {
    std::string bin;
    for (std::size_t i = 0; i < vertices; ++i) {
        bin += le_bytes<float>({0.f, float(i), 0.f});
    }
    const auto positions_size = bin.size();
    while (extra.size() % 4 != 0) { extra += '\0'; }
    bin += extra;
    return load_glb(
        dir,
        std::format(R"("buffers":[{{"byteLength":{}}}],)"
                    R"("bufferViews":[{{"buffer":0,"byteLength":{}}})"
                    R"({}],{})",
                    bin.size(),
                    positions_size,
                    extra.empty()
                        ? std::string()
                        : std::format(R"(,{{"buffer":0,"byteOffset":{},"byteLength":{}}})",
                                      positions_size,
                                      extra.size()),
                    rest),
        bin);
}

auto triangles(const balsa::scene_graph::MeshGeometry &geometry)
    -> std::vector<std::array<std::uint32_t, 3>> {
    const auto t = geometry.triangle_indices();
    return std::views::iota(std::size_t{0}, t.extent(1))
           | std::views::transform([&](std::size_t i) {
                 return std::array{t[0, i], t[1, i], t[2, i]};
             })
           | std::ranges::to<std::vector>();
}

auto edges(const balsa::scene_graph::MeshGeometry &geometry)
    -> std::vector<std::array<std::uint32_t, 2>> {
    const auto e = geometry.edge_indices();
    return std::views::iota(std::size_t{0}, e.extent(1))
           | std::views::transform([&](std::size_t i) {
                 return std::array{e[0, i], e[1, i]};
             })
           | std::ranges::to<std::vector>();
}
} // namespace

TEST_CASE("gltf_mesh_conversion", "[gltf][mesh]") {
    TempDir dir;
    namespace sg = balsa::scene_graph;
    using Tri = std::array<std::uint32_t, 3>;
    using Edge = std::array<std::uint32_t, 2>;

    SECTION("indexed triangles with normalized colors") {
        // 3 vertices; extra: 3 RGBA8 colors, then 3 uint16 indices.
        auto extra = le_bytes<std::uint8_t>({255, 0, 0, 255, 0, 255, 0, 255,
                                             0, 0, 255, 128});
        extra += le_bytes<std::uint16_t>({2, 1, 0});
        const auto doc = load_mesh_glb(
            dir, 3, extra,
            std::format(
                R"("accessors":[{}3}},)"
                R"({{"bufferView":1,"componentType":5121,"normalized":true,"type":"VEC4","count":3}},)"
                R"({{"bufferView":1,"byteOffset":12,"componentType":5123,"type":"SCALAR","count":3}}],)"
                R"("materials":[{{}}],)"
                R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":0,"COLOR_0":1}},)"
                R"("indices":2,"material":0}}]}}])",
                positions_accessor));
        REQUIRE(doc);
        const auto geometry = gltf::to_mesh_geometry(*doc, 0, 0);
        REQUIRE(geometry);
        CHECK(triangles(*geometry) == std::vector<Tri>{{2, 1, 0}});

        const auto positions = geometry->find_attribute("vertex_positions");
        REQUIRE(positions);
        CHECK(positions->type() == sg::AttributeType::Float32);
        CHECK((*positions->view<float, 3>())[1, 2] == 2.f);

        const auto colors = geometry->find_attribute("COLOR_0");
        REQUIRE(colors);
        // Kept as normalized bytes.
        CHECK(colors->type() == sg::AttributeType::UInt8);
        CHECK(colors->normalized());
        const auto c = *colors->view<std::uint8_t, 4>();
        CHECK(c[0, 0] == 255);
        CHECK(c[1, 1] == 255);
        CHECK(c[3, 2] == 128);
        CHECK_FALSE(positions->normalized());

        // Tagged by their glTF semantics.
        CHECK(geometry->attribute_for(sg::semantics::position()) == positions);
        CHECK(geometry->attribute_for(sg::semantics::color(), 0) == colors);

        const auto all = gltf::to_mesh_geometries(*doc, 0);
        REQUIRE(all);
        REQUIRE(all->size() == 1);
        CHECK((*all)[0].mode == gltf::PrimitiveMode::TRIANGLES);
        CHECK((*all)[0].material == 0);
    }

    SECTION("modes without indices") {
        const auto convert = [&](int mode, std::size_t vertices) {
            const auto doc = load_mesh_glb(
                dir, vertices, "",
                std::format(R"("accessors":[{}{}}}],)"
                            R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":0}},)"
                            R"("mode":{}}}]}}])",
                            positions_accessor,
                            vertices,
                            mode));
            REQUIRE(doc);
            auto geometry = gltf::to_mesh_geometry(*doc, 0, 0);
            REQUIRE(geometry);
            return std::move(*geometry);
        };
        CHECK(triangles(convert(4, 6)) == std::vector<Tri>{{0, 1, 2}, {3, 4, 5}});
        // Strips alternate winding: {0,1,2}, {1,3,2}, {2,3,4}.
        CHECK(triangles(convert(5, 5))
              == std::vector<Tri>{{0, 1, 2}, {1, 3, 2}, {2, 3, 4}});
        CHECK(triangles(convert(6, 4)) == std::vector<Tri>{{1, 2, 0}, {2, 3, 0}});
        CHECK(edges(convert(1, 4)) == std::vector<Edge>{{0, 1}, {2, 3}});
        CHECK(edges(convert(3, 3)) == std::vector<Edge>{{0, 1}, {1, 2}});
        CHECK(edges(convert(2, 3)) == std::vector<Edge>{{0, 1}, {1, 2}, {2, 0}});
        const auto points = convert(0, 3);
        CHECK_FALSE(points.has_triangle_indices());
        CHECK_FALSE(points.has_edge_indices());
        CHECK(points.find_attribute("vertex_positions"));
    }

    SECTION("invalid primitives fail conversion") {
        const auto message = [&](std::size_t vertices, std::string extra,
                                 std::string_view primitive) {
            const auto doc = load_mesh_glb(
                dir, vertices, std::move(extra),
                std::format(R"("accessors":[{}{}}},)"
                            R"({{"bufferView":1,"componentType":5121,"type":"SCALAR","count":4}}],)"
                            R"("meshes":[{{"primitives":[{}]}}])",
                            positions_accessor,
                            vertices,
                            primitive));
            REQUIRE(doc);
            const auto geometry = gltf::to_mesh_geometries(*doc, 0);
            REQUIRE_FALSE(geometry);
            return geometry.error().message;
        };
        CHECK_THAT(message(3, le_bytes<std::uint8_t>({0, 1, 2, 7}),
                           R"({"attributes":{"POSITION":0},"indices":1,"mode":1})"),
                   ContainsSubstring("has index 7 but 3 vertices"));
        CHECK_THAT(message(3, le_bytes<std::uint8_t>({0, 1, 2, 0}),
                           R"({"attributes":{"POSITION":0},"indices":1})"),
                   ContainsSubstring("4 TRIANGLES indices, not a multiple of 3"));
    }

    SECTION("invalid meshes fail load") {
        const auto message = [&](std::string_view meshes) {
            const auto doc = load_mesh_glb(
                dir, 3, le_bytes<float>({1}),
                std::format(R"("accessors":[{}3}},)"
                            R"({{"bufferView":1,"componentType":5126,"type":"SCALAR","count":1}}],)"
                            R"("meshes":[{}])",
                            positions_accessor,
                            meshes));
            REQUIRE_FALSE(doc);
            return doc.error().message;
        };
        CHECK_THAT(message(R"({"primitives":[{"attributes":{"POSITION":9}}]})"),
                   ContainsSubstring("attribute POSITION refers to missing accessor 9"));
        CHECK_THAT(message(R"({"primitives":[{"attributes":{"POSITION":0,"_X":1}}]})"),
                   ContainsSubstring("attribute _X has 1 values, but other attributes have 3"));
        CHECK_THAT(message(R"({"primitives":[{"attributes":{"POSITION":0},"indices":1}]})"),
                   ContainsSubstring("not an unsigned integer scalar"));
        CHECK_THAT(message(R"({"primitives":[{"attributes":{"POSITION":0},"material":2}]})"),
                   ContainsSubstring("missing material 2"));
    }
}

TEST_CASE("gltf_scene_conversion", "[gltf][scene]") {
    TempDir dir;
    namespace sg = balsa::scene_graph;
    // Accessor 0: 3 positions. Mesh 0: one triangle. Mesh 1: two primitives.
    const auto load = [&](std::string_view nodes_and_scenes) {
        return load_mesh_glb(
            dir, 3, "",
            std::format(R"("accessors":[{}3}}],)"
                        R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":0}}}}]}},)"
                        R"({{"primitives":[{{"attributes":{{"POSITION":0}}}},)"
                        R"({{"attributes":{{"POSITION":0}},"mode":0}}]}}],{})",
                        positions_accessor,
                        nodes_and_scenes));
    };
    const auto mesh_of = [](const sg::Object &object) {
        const auto *instance = object.find_feature<sg::MeshInstance>();
        return instance ? instance->shared_geometry() : nullptr;
    };

    SECTION("hierarchy, transforms and shared meshes") {
        const auto doc = load(
            R"("nodes":[)"
            R"({"name":"root","translation":[1,2,3],"children":[1,2]},)"
            R"({"name":"a","mesh":0,"scale":[2,2,2],"rotation":[0,0,0.7071068,0.7071068]},)"
            R"({"name":"b","mesh":0,"matrix":[1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1]},)"
            R"({"name":"multi","mesh":1}],)"
            R"("scenes":[{"name":"other","nodes":[3]},{"name":"main","nodes":[0]}],)"
            R"("scene":1)");
        REQUIRE(doc);
        const auto root = gltf::to_object(*doc);
        REQUIRE(root);
        const auto &scene = **root;
        CHECK(scene.name == "main"); // the default scene
        REQUIRE(scene.children_count() == 1);
        const auto &node = *scene.children()[0];
        CHECK(node.name == "root");
        CHECK(node.translation() == sg::Vec3f{1.f, 2.f, 3.f});
        CHECK_FALSE(mesh_of(node));
        REQUIRE(node.children_count() == 2);

        const auto &a = *node.children()[0];
        CHECK(a.name == "a");
        CHECK(a.scale_factors() == sg::Vec3f{2.f, 2.f, 2.f});
        // glTF quaternions are [x, y, z, w]: 90 degrees about z.
        CHECK(a.rotation().w() == Catch::Approx(0.7071068f));
        CHECK(a.rotation().z() == Catch::Approx(0.7071068f));
        // World transform: (1,0,0) -> scale 2 -> rotate to (0,2,0) -> +(1,2,3).
        const auto p = (a.world_transform()
                        * ::zipper::Vector<float, 3>{1.f, 0.f, 0.f}.homogeneous())
                           .eval();
        CHECK(p(0) == Catch::Approx(1.f).margin(1e-5f));
        CHECK(p(1) == Catch::Approx(4.f).margin(1e-5f));
        CHECK(p(2) == Catch::Approx(3.f).margin(1e-5f));

        const auto &b = *node.children()[1];
        CHECK(b.translation() == sg::Vec3f{5.f, 6.f, 7.f}); // from the matrix

        // Both nodes show mesh 0's single primitive, on their own Object,
        // through the same geometry.
        REQUIRE(mesh_of(a));
        CHECK(mesh_of(a) == mesh_of(b));
        CHECK(mesh_of(a)->triangle_count() == 1);

        const auto other = gltf::to_object(*doc, 0);
        REQUIRE(other);
        const auto &multi = *(*other)->children()[0];
        CHECK(multi.name == "multi");
        CHECK_FALSE(mesh_of(multi));
        REQUIRE(multi.children_count() == 2);
        CHECK(multi.children()[0]->name == "primitive 0");
        CHECK(mesh_of(*multi.children()[0])->triangle_count() == 1);
        CHECK(mesh_of(*multi.children()[1])->triangle_count() == 0); // POINTS

        CHECK_FALSE(gltf::to_object(*doc, 5));
    }

    SECTION("without scenes, every root node") {
        const auto doc = load(R"("nodes":[{"children":[1]},{},{"mesh":0}])");
        REQUIRE(doc);
        const auto root = gltf::to_object(*doc);
        REQUIRE(root);
        REQUIRE((*root)->children_count() == 2);
        CHECK((*root)->children()[0]->name == "node 0");
        CHECK((*root)->children()[0]->children_count() == 1);
        CHECK((*root)->children()[1]->name == "node 2");
    }

    SECTION("invalid node graphs fail load") {
        const auto message = [&](std::string_view json) {
            const auto doc = load(json);
            REQUIRE_FALSE(doc);
            return doc.error().message;
        };
        CHECK_THAT(message(R"("nodes":[{"children":[2]},{"children":[2]},{}])"),
                   ContainsSubstring("node 2 has more than one parent"));
        CHECK_THAT(message(R"("nodes":[{},{"children":[2]},{"children":[1]}])"),
                   ContainsSubstring("is part of a cycle"));
        CHECK_THAT(message(R"("nodes":[{"children":[5]}])"),
                   ContainsSubstring("refers to missing child node 5"));
        CHECK_THAT(message(R"("nodes":[{"mesh":9}])"),
                   ContainsSubstring("refers to missing mesh 9"));
        CHECK_THAT(message(R"("nodes":[{"children":[1]},{}],"scenes":[{"nodes":[1]}])"),
                   ContainsSubstring("not a root node"));
        CHECK_THAT(message(R"("nodes":[{}],"scenes":[{"nodes":[0]}],"scene":3)"),
                   ContainsSubstring("default scene 3 does not exist"));
    }
}

TEST_CASE("load_into_mesh_scene", "[gltf][viewer]") {
    TempDir dir;
    namespace sg = balsa::scene_graph;
    namespace vk = balsa::visualization::vulkan;
    vk::MeshScene scene;
    const auto loader = sg::SceneLoader::defaults();

    const auto drawable_count = [](this const auto &self,
                                   const sg::Object &object) -> std::size_t {
        std::size_t n = object.find_feature<vk::VulkanMeshDrawable>() ? 1 : 0;
        for (const auto &child : object.children()) { n += self(*child); }
        return n;
    };
    // Primitives without explicit materials use the default Phong surface.
    const auto surface_enabled = [](const sg::Object &object,
                                    std::string_view renderer = "balsa.phong_surface") {
        const auto *shading = object.find_feature<sg::BasicShading>();
        REQUIRE(shading);
        const auto *layer = shading->find_layer(renderer);
        REQUIRE(layer);
        return layer->enabled;
    };

    SECTION("glTF scene") {
        // One triangle (0,0,0) (0,1,0) (0,2,0), shown by two nodes: one at
        // the origin and one translated by (10, 0, 0).
        std::string bin;
        for (int i = 0; i < 3; ++i) { bin += le_bytes<float>({0.f, float(i), 0.f}); }
        const auto path = dir.write(
            "scene.glb",
            make_glb(std::format(
                         R"({{"asset":{{"version":"2.0"}},"buffers":[{{"byteLength":36}}],)"
                         R"("bufferViews":[{{"buffer":0,"byteLength":36}}],)"
                         R"("accessors":[{}3}}],)"
                         R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":0}}}}]}}],)"
                         R"("nodes":[{{"mesh":0}},{{"mesh":0,"translation":[10,0,0]}}],)"
                         R"("scenes":[{{"nodes":[0,1]}}]}})",
                         positions_accessor),
                     bin));
        auto loaded = loader.load(path);
        REQUIRE(loaded);
        CHECK((*loaded)->parent() == nullptr);
        CHECK(drawable_count(**loaded) == 0); // until added to a scene
        const auto &object = scene.add_object(std::move(*loaded));
        CHECK(object.name == "scene.glb");
        CHECK(object.parent() == &scene.root());
        CHECK(object.children_count() == 2);
        CHECK(drawable_count(object) == 2);
        CHECK(surface_enabled(*object.children()[0]));

        // Loaded as is: no rescaling, so the world box is the file's.
        const auto box = balsa::visualization::world_bounding_box(object);
        CHECK(box.min()(0) == Catch::Approx(0.f));
        CHECK(box.max()(0) == Catch::Approx(10.f));
        CHECK(box.max()(1) == Catch::Approx(2.f));
    }

    SECTION("only explicitly specified glTF materials use metallic-roughness") {
        std::string bin;
        for (int i = 0; i < 3; ++i) {
            bin += le_bytes<float>({0.f, float(i), 0.f});
        }
        const auto path = dir.write(
            "materials.glb",
            make_glb(
                std::format(
                    R"({{"asset":{{"version":"2.0"}},"buffers":[{{"byteLength":36}}],)"
                    R"("bufferViews":[{{"buffer":0,"byteLength":36}}],)"
                    R"("accessors":[{}3}}],)"
                    R"("materials":[{{}},{{"pbrMetallicRoughness":{{"baseColorFactor":[0.2,0.3,0.4,1],"metallicFactor":0.25,"roughnessFactor":0.5}}}}],)"
                    R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":0}}}},{{"attributes":{{"POSITION":0}},"material":0}},{{"attributes":{{"POSITION":0}},"material":1}}]}}],)"
                    R"("nodes":[{{"mesh":0}}]}})",
                    positions_accessor),
                bin));
        auto loaded = loader.load(path);
        REQUIRE(loaded);
        REQUIRE((*loaded)->children_count() == 1);
        const auto &primitives = (*loaded)->children()[0]->children();
        REQUIRE(primitives.size() == 3);

        const auto *shading = primitives[0]->find_feature<sg::BasicShading>();
        REQUIRE(shading);
        const auto *surface = shading->find_layer("balsa.phong_surface");
        REQUIRE(surface);
        const auto *phong = dynamic_cast<const sg::PhongSurfaceRenderer *>(
            surface->renderer.get());
        REQUIRE(phong);
        CHECK(surface->enabled);
        const sg::PhongSurfaceRenderer defaults;
        for (int i = 0; i < 4; ++i) {
            CHECK(phong->state.uniform_color(i)
                  == defaults.state.uniform_color(i));
        }
        CHECK(phong->state.material.ambient_strength
              == defaults.state.material.ambient_strength);
        CHECK(phong->state.material.diffuse_strength
              == defaults.state.material.diffuse_strength);
        CHECK(phong->state.material.specular_strength
              == defaults.state.material.specular_strength);
        CHECK(phong->state.material.shininess
              == defaults.state.material.shininess);

        const auto material = [&](std::size_t index) {
            const auto *shading =
                primitives[index]->find_feature<sg::BasicShading>();
            REQUIRE(shading);
            const auto *layer =
                shading->find_layer("balsa.gltf_metallic_roughness_surface");
            REQUIRE(layer);
            CHECK(layer->enabled);
            const auto *renderer =
                dynamic_cast<const sg::GltfMetallicRoughnessRenderer *>(
                    layer->renderer.get());
            REQUIRE(renderer);
            return renderer->material;
        };
        const auto empty = material(1);
        for (int i = 0; i < 4; ++i) { CHECK(empty->base_color(i) == 1.0f); }
        CHECK(empty->metallic == 1.0f);
        CHECK(empty->roughness == 1.0f);
        const auto colored = material(2);
        CHECK(colored->base_color(0) == Catch::Approx(0.2f));
        CHECK(colored->base_color(1) == Catch::Approx(0.3f));
        CHECK(colored->base_color(2) == Catch::Approx(0.4f));
        CHECK(colored->metallic == 0.25f);
        CHECK(colored->roughness == 0.5f);
    }

    SECTION("OBJ mesh") {
        const auto path = dir.write("tri.OBJ", "v 0 0 0\nv 2 0 0\nv 0 1 0\nf 1 2 3\n");
        auto loaded = loader.load(path);
        REQUIRE(loaded);
        CHECK((*loaded)->name == "tri.OBJ");
        {
            const auto *instance = (*loaded)->find_feature<sg::MeshInstance>();
            REQUIRE(instance);
            const auto positions =
                instance->geometry()->attribute_for(sg::semantics::position());
            REQUIRE(positions);
            CHECK(positions->name == "vertex_positions");
        }
        CHECK_FALSE((*loaded)->find_feature<sg::BasicShading>());
        const auto *instance = (*loaded)->find_feature<sg::MeshInstance>();
        REQUIRE(instance);
        CHECK(instance->geometry()->triangle_count() == 1);
        CHECK((*loaded)->scale_factors()(0) == Catch::Approx(1.f));
        const auto &object = scene.add_object(std::move(*loaded));
        CHECK(drawable_count(object) == 1);
        CHECK(surface_enabled(object));
    }

    SECTION("edge-only meshes start as wireframe") {
        auto object = std::make_unique<sg::Object>("edges");
        auto geometry = std::make_shared<sg::MeshGeometry>();
        const std::vector<uint32_t> edges{0, 1};
        geometry->set_edge_indices(
            ::zipper::mdspan<const uint32_t, ::zipper::dextents<2>,
                             ::zipper::storage::col_major>(edges.data(), 2, 1));
        object->emplace_feature<sg::MeshInstance>(geometry);
        CHECK_FALSE(surface_enabled(scene.add_object(std::move(object))));
    }

    SECTION("errors") {
        CHECK_THAT(loader.load(dir.path("missing.obj")).error(),
                   ContainsSubstring("file not found"));
        const auto bad = dir.write("bad.gltf", R"({"asset":{"version":"3.0"}})");
        CHECK_THAT(loader.load(bad).error(),
                   ContainsSubstring("only 2.0 is supported"));
    }
}

TEST_CASE("orbit_camera_frames_box", "[viewer]") {
    namespace vk = balsa::visualization::vulkan;
    vk::MeshScene scene;
    scene.set_perspective(1.0f, 1.5f, 0.01f, 100.f);
    vk::OrbitCameraController camera(&scene);

    // x in [0, 10], y in [0, 2], flat in z.
    balsa::geometry::BoundingBox<float, 3> box;
    box.expand(::zipper::Vector<float, 3>{0.f, 0.f, 0.f});
    box.expand(::zipper::Vector<float, 3>{10.f, 2.f, 0.f});
    camera.frame(box);

    const float radius = 0.5f * std::sqrt(104.f);
    CHECK(camera.center()(0) == Catch::Approx(5.f));
    CHECK(camera.center()(1) == Catch::Approx(1.f));
    CHECK(camera.distance()
          == Catch::Approx(1.05f * radius / std::sin(0.5f)));
    CHECK(scene.camera().fov_y() == Catch::Approx(1.0f));
    CHECK(scene.camera().aspect() == Catch::Approx(1.5f));
    CHECK(scene.camera().near_plane() == Catch::Approx(1e-3f * radius));
    CHECK(scene.camera().far_plane() > camera.distance() + radius);

    // Empty boxes leave the view alone.
    const auto distance = camera.distance();
    camera.frame({});
    CHECK(camera.distance() == distance);
}

namespace {
auto touch(balsa::visualization::TouchEvent::Type type,
           std::vector<balsa::visualization::TouchPoint> points)
  -> balsa::visualization::TouchEvent {
    return { type, std::move(points), 0 };
}
}// namespace

TEST_CASE("touch_gesture_tracker", "[viewer][touch]") {
    namespace viz = balsa::visualization;
    using T = viz::TouchEvent::Type;
    using S = viz::TouchPoint::State;
    viz::TouchGestureTracker tracker;

    // The first event only establishes the reference.
    CHECK_FALSE(tracker.update(touch(T::Begin, { { 0, 10, 10, S::Pressed } })));
    auto g = tracker.update(touch(T::Update, { { 0, 15, 7, S::Moved } }));
    REQUIRE(g);
    CHECK(g->touch_count == 1);
    CHECK(g->translation(0) == Catch::Approx(5.0));
    CHECK(g->translation(1) == Catch::Approx(-3.0));
    CHECK(g->scale == 1.0);

    // A second finger re-bases instead of producing a jump.
    CHECK_FALSE(tracker.update(touch(T::Update, { { 0, 15, 7, S::Stationary }, { 1, 25, 7, S::Pressed } })));
    CHECK(tracker.touch_count() == 2);

    // Spreading symmetrically: pure pinch, no translation.
    g = tracker.update(touch(T::Update, { { 0, 10, 7, S::Moved }, { 1, 30, 7, S::Moved } }));
    REQUIRE(g);
    CHECK(g->touch_count == 2);
    CHECK(g->scale == Catch::Approx(2.0));
    CHECK(g->translation(0) == Catch::Approx(0.0));

    // Moving both together: pure translation.
    g = tracker.update(touch(T::Update, { { 0, 10, 17, S::Moved }, { 1, 30, 17, S::Moved } }));
    REQUIRE(g);
    CHECK(g->scale == Catch::Approx(1.0));
    CHECK(g->translation(1) == Catch::Approx(10.0));

    // Lifting a finger re-bases again.
    CHECK_FALSE(tracker.update(touch(T::Update, { { 0, 10, 17, S::Stationary }, { 1, 30, 17, S::Released } })));
    CHECK(tracker.touch_count() == 1);

    CHECK_FALSE(tracker.update(touch(T::Cancel, {})));
    CHECK(tracker.touch_count() == 0);
}

TEST_CASE("orbit_camera_touch", "[viewer][touch]") {
    namespace vk = balsa::visualization::vulkan;
    namespace viz = balsa::visualization;
    using T = viz::TouchEvent::Type;
    using S = viz::TouchPoint::State;
    vk::MeshScene scene;
    vk::OrbitCameraController camera(&scene);

    SECTION("one finger orbits") {
        const float theta = camera.theta();
        camera.on_touch_event(touch(T::Begin, { { 0, 0, 0, S::Pressed } }));
        camera.on_touch_event(touch(T::Update, { { 0, 100, 0, S::Moved } }));
        CHECK(camera.theta() == Catch::Approx(theta - 100 * camera.orbit_sensitivity));
        CHECK(camera.distance() == Catch::Approx(3.f));
    }

    SECTION("pinch zooms") {
        camera.on_touch_event(touch(T::Begin, { { 0, 40, 50, S::Pressed }, { 1, 60, 50, S::Pressed } }));
        camera.on_touch_event(touch(T::Update, { { 0, 30, 50, S::Moved }, { 1, 70, 50, S::Moved } }));
        CHECK(camera.distance() == Catch::Approx(1.5f));
        CHECK(camera.center().norm() == Catch::Approx(0.f).margin(1e-6));
    }

    SECTION("two finger drag pans") {
        const float theta = camera.theta();
        camera.on_touch_event(touch(T::Begin, { { 0, 40, 50, S::Pressed }, { 1, 60, 50, S::Pressed } }));
        camera.on_touch_event(touch(T::Update, { { 0, 40, 80, S::Moved }, { 1, 60, 80, S::Moved } }));
        CHECK(camera.theta() == theta);
        CHECK(camera.center()(1) > 0.f);
        CHECK(camera.distance() == Catch::Approx(3.f));
    }
}

TEST_CASE("orbit_camera_screen_space_tracking", "[viewer][touch]") {
    namespace vk = balsa::visualization::vulkan;
    vk::MeshScene scene;
    const float fov = 1.0f;
    scene.set_perspective(fov, 2.0f, 0.01f, 100.f);
    vk::OrbitCameraController camera(&scene);
    camera.set_viewport_size(800, 400);
    // Looking down -z: screen +x is world +x, screen +y is world -y.
    camera.set_angles(0.f, 0.f);
    const float wpp0 = 2.f * camera.distance() * std::tan(0.5f * fov) / 400.f;

    SECTION("pan moves one pixel's worth per pixel") {
        camera.pan(10, 20);
        CHECK(camera.center()(0) == Catch::Approx(-10 * wpp0));
        CHECK(camera.center()(1) == Catch::Approx(20 * wpp0));
    }

    SECTION("zoom_at keeps the point under the cursor fixed") {
        const double ox = 100, oy = -50;// offset from viewport center
        const float d0 = camera.distance();
        // World point on the center plane under the cursor.
        const float px = camera.center()(0) + static_cast<float>(ox) * wpp0;
        const float py = camera.center()(1) - static_cast<float>(oy) * wpp0;

        camera.zoom_at(2.f, 400 + ox, 200 + oy);
        CHECK(camera.distance() == Catch::Approx(0.5f * d0));
        const float wpp1 = 2.f * camera.distance() * std::tan(0.5f * fov) / 400.f;
        CHECK(camera.center()(0) + static_cast<float>(ox) * wpp1 == Catch::Approx(px));
        CHECK(camera.center()(1) - static_cast<float>(oy) * wpp1 == Catch::Approx(py));
    }

    SECTION("zoom_at the viewport center does not pan") {
        camera.zoom_at(2.f, 400, 200);
        CHECK(camera.center().norm() == Catch::Approx(0.f).margin(1e-6));
    }
}

TEST_CASE("orbit_camera_drops_stale_drags", "[viewer]") {
    namespace vk = balsa::visualization::vulkan;
    namespace viz = balsa::visualization;
    vk::MeshScene scene;
    vk::OrbitCameraController camera(&scene);
    const float theta = camera.theta();

    auto mouse = [](viz::MouseEvent::Type type, double x, int button, int buttons) {
        viz::MouseEvent e{ type };
        e.x = x;
        e.button = button;
        e.buttons = buttons;
        return e;
    };
    camera.on_mouse_event(mouse(viz::MouseEvent::Type::Press, 0, 0, 1));

    SECTION("focus loss ends the drag") {
        camera.on_focus_lost();
        camera.on_mouse_event(mouse(viz::MouseEvent::Type::Move, 50, -1, -1));
        CHECK(camera.theta() == theta);
    }

    SECTION("a move with no held buttons ends the drag") {
        camera.on_mouse_event(mouse(viz::MouseEvent::Type::Move, 50, -1, 0));
        CHECK(camera.theta() == theta);
    }

    SECTION("held buttons keep dragging") {
        camera.on_mouse_event(mouse(viz::MouseEvent::Type::Move, 50, -1, 1));
        CHECK(camera.theta() != theta);
    }
}

TEST_CASE("camera_fov_spans_shorter_side", "[viewer]") {
    balsa::scene_graph::Camera cam;
    cam.set_perspective(1.0f, 2.0f, 0.1f, 10.f);
    CHECK(cam.vertical_fov() == Catch::Approx(1.0f));

    // Vertical (default): portrait keeps the vertical angle.
    cam.update_aspect(0.5f);
    CHECK(cam.vertical_fov() == Catch::Approx(1.0f));

    // ShorterSide: in portrait the horizontal angle stays at fov.
    cam.set_fov_axis(balsa::scene_graph::Camera::FovAxis::ShorterSide);
    const float horizontal = 2.f * std::atan(std::tan(0.5f * cam.vertical_fov()) * 0.5f);
    CHECK(horizontal == Catch::Approx(1.0f));
    cam.update_aspect(2.0f);
    CHECK(cam.vertical_fov() == Catch::Approx(1.0f));
}

TEST_CASE("orbit_camera_touchpad", "[viewer][touch]") {
    namespace vk = balsa::visualization::vulkan;
    namespace viz = balsa::visualization;
    using T = viz::TouchpadEvent::Type;
    vk::MeshScene scene;
    vk::OrbitCameraController camera(&scene);
    camera.set_viewport_size(800, 400);
    const float theta = camera.theta();
    const float distance = camera.distance();

    SECTION("scroll orbits") {
        camera.on_touchpad_event({ .type = T::Scroll, .dx = 40 });
        CHECK(camera.theta() == Catch::Approx(theta - 40 * camera.orbit_sensitivity));
        CHECK(camera.distance() == distance);
    }

    SECTION("shift scroll pans") {
        camera.on_touchpad_event({ .type = T::Scroll, .dx = 40, .modifiers = viz::ModShift });
        CHECK(camera.theta() == theta);
        CHECK(camera.center().norm() > 0.f);
    }

    SECTION("ctrl scroll zooms") {
        camera.on_touchpad_event({ .type = T::Scroll, .x = 400, .y = 200, .dy = 50, .modifiers = viz::ModControl });
        CHECK(camera.distance() < distance);
    }

    SECTION("pinch zooms at the cursor") {
        camera.on_touchpad_event({ .type = T::Pinch, .x = 400, .y = 200, .scale = 2.0 });
        CHECK(camera.distance() == Catch::Approx(0.5f * distance));
        CHECK(camera.center().norm() == Catch::Approx(0.f).margin(1e-6));
    }
}

TEST_CASE("touch_gesture_rotation", "[viewer][touch]") {
    namespace viz = balsa::visualization;
    using T = viz::TouchEvent::Type;
    using S = viz::TouchPoint::State;
    viz::TouchGestureTracker tracker;
    tracker.update(touch(T::Begin, { { 0, 90, 100, S::Pressed }, { 1, 110, 100, S::Pressed } }));
    // Quarter turn about (100, 100): left/right -> top/bottom.  With y down,
    // left -> top is clockwise on screen.
    auto g = tracker.update(touch(T::Update, { { 0, 100, 90, S::Moved }, { 1, 100, 110, S::Moved } }));
    REQUIRE(g);
    CHECK(g->rotation == Catch::Approx(std::numbers::pi / 2));
    CHECK(g->scale == Catch::Approx(1.0));
    CHECK(g->translation.norm() == Catch::Approx(0.0).margin(1e-12));

    // Crossing the +-pi seam wraps instead of jumping by 2pi.
    tracker.update(touch(T::Update, { { 0, 110, 99, S::Moved }, { 1, 90, 101, S::Moved } }));
    g = tracker.update(touch(T::Update, { { 0, 110, 101, S::Moved }, { 1, 90, 99, S::Moved } }));
    REQUIRE(g);
    CHECK(std::abs(g->rotation) < 0.2);
}

TEST_CASE("orbit_camera_roll", "[viewer][touch]") {
    namespace vk = balsa::visualization::vulkan;
    vk::MeshScene scene;
    const float fov = 1.0f;
    scene.set_perspective(fov, 2.0f, 0.01f, 100.f);
    vk::OrbitCameraController camera(&scene);
    camera.set_viewport_size(800, 400);
    camera.set_angles(0.f, 0.f);// looking down -z, screen right = +x

    auto camera_axis = [&](int i) {
        return balsa::scene_graph::Vec3f(
          scene.active_camera_object().world_transform().linear().col(i));
    };

    SECTION("positive roll turns content clockwise") {
        camera.set_roll(std::numbers::pi_v<float> / 2);
        // World +y (was screen-up) now points screen-right.
        CHECK(camera_axis(0)(1) == Catch::Approx(1.f));
        CHECK(camera_axis(1)(0) == Catch::Approx(-1.f));
    }

    SECTION("roll_at keeps the point under the cursor fixed") {
        const float wpp = 2.f * camera.distance() * std::tan(0.5f * fov) / 400.f;
        // World point under screen offset (100, 0): +x by 100 px.
        const float px = 100 * wpp;
        camera.roll_at(std::numbers::pi / 2, 500, 200);
        // After a clockwise quarter turn screen-right is world +y, screen-down
        // is world +x; the point should still be 100 px right of center.
        const auto [right, up] = std::pair{ camera_axis(0), camera_axis(1) };
        const auto point = balsa::scene_graph::Vec3f{ px, 0.f, 0.f };
        const auto offset = point - camera.center();
        CHECK(offset.dot(right) / wpp == Catch::Approx(100.f));
        CHECK(offset.dot(up) / wpp == Catch::Approx(0.f).margin(1e-3));
    }

    SECTION("orbit follows the cursor when rolled") {
        camera.set_roll(std::numbers::pi_v<float> / 2);
        // Screen-right is now world-up: a rightward drag tilts (phi), not spins.
        const float theta = camera.theta();
        camera.orbit(50, 0);
        CHECK(camera.theta() == Catch::Approx(theta).margin(1e-6));
        CHECK(camera.phi() != 0.f);
    }

    SECTION("frame resets roll") {
        camera.set_roll(1.f);
        balsa::geometry::BoundingBox<float, 3> box;
        box.expand(::zipper::Vector<float, 3>{ 0.f, 0.f, 0.f });
        box.expand(::zipper::Vector<float, 3>{ 1.f, 1.f, 1.f });
        camera.frame(box);
        CHECK(camera.roll() == 0.f);
    }
}

TEST_CASE("scene_loader", "[viewer]") {
    TempDir dir;
    namespace sg = balsa::scene_graph;

    const auto defaults = sg::SceneLoader::defaults();
    CHECK(defaults.handles(".obj"));
    CHECK(defaults.handles(".OBJ"));
    CHECK(defaults.handles(".glb"));
    CHECK_FALSE(defaults.handles(".ply"));

    // Extensions match case-insensitively; copies are independent.
    int calls = 0;
    auto custom = defaults;
    custom.add(".PLY", [&](const std::filesystem::path &path) {
        ++calls;
        return sg::SceneLoader::result_type(
            std::make_unique<sg::Object>(path.filename().string()));
    });
    CHECK(custom.handles(".ply"));
    CHECK_FALSE(defaults.handles(".ply"));

    const auto ply = dir.write("points.Ply", "ply\n");
    const auto loaded = custom.load(ply);
    REQUIRE(loaded);
    CHECK((*loaded)->name == "points.Ply");
    CHECK(calls == 1);

    // Replacing a loader, chained on a temporary.
    const auto copy = sg::SceneLoader(custom).add(
        ".ply", [](const std::filesystem::path &) {
            return sg::SceneLoader::result_type(std::unexpected("replaced"));
        });
    CHECK(copy.load(ply).error() == "replaced");
    CHECK(custom.load(ply));
    CHECK(calls == 2);

    CHECK_THAT(defaults.load(ply).error(),
               ContainsSubstring("unsupported file type"));
    CHECK_THAT(defaults.load(dir.path("missing.obj")).error(),
               ContainsSubstring("file not found"));
}

TEST_CASE("gltf_attribute_semantics", "[gltf]") {
    namespace sg = balsa::scene_graph;
    using S = gltf::AttributeSemanticSet;
    CHECK(gltf::attribute_semantic("POSITION") == S{sg::semantics::position()});
    CHECK(gltf::attribute_semantic("NORMAL") == S{sg::semantics::normal()});
    CHECK(gltf::attribute_semantic("TANGENT") == S{sg::semantics::tangent()});
    CHECK(gltf::attribute_semantic("TEXCOORD_0")
          == S{sg::semantics::texture_coordinates(), 0});
    CHECK(gltf::attribute_semantic("TEXCOORD_1")
          == S{sg::semantics::texture_coordinates(), 1});
    CHECK(gltf::attribute_semantic("COLOR_2") == S{sg::semantics::color(), 2});
    // Untagged: skinning, application-specific and malformed semantics.
    CHECK_FALSE(gltf::attribute_semantic("JOINTS_0"));
    CHECK_FALSE(gltf::attribute_semantic("WEIGHTS_0"));
    CHECK_FALSE(gltf::attribute_semantic("_TEMPERATURE"));
    CHECK_FALSE(gltf::attribute_semantic("TEXCOORD_"));
    CHECK_FALSE(gltf::attribute_semantic("TEXCOORD_x"));
    CHECK_FALSE(gltf::attribute_semantic("COLOR_999"));
}

#if defined(BALSA_VISUALIZATION_MESHOPT)
#include <meshoptimizer.h>

TEST_CASE("gltf_meshopt_compression", "[gltf]") {
    TempDir dir;
    namespace sg = balsa::scene_graph;
    // A quad: 4 positions, 2 triangles.
    const std::array<float, 12> positions{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    const std::array<unsigned, 6> indices{0, 1, 2, 0, 2, 3};

    std::vector<unsigned char> vertices(meshopt_encodeVertexBufferBound(4, 12));
    vertices.resize(meshopt_encodeVertexBuffer(vertices.data(), vertices.size(),
                                               positions.data(), 4, 12));
    std::vector<unsigned char> triangles_data(meshopt_encodeIndexBufferBound(6, 4));
    triangles_data.resize(meshopt_encodeIndexBuffer(triangles_data.data(),
                                                    triangles_data.size(),
                                                    indices.data(), 6));
    std::string bin(reinterpret_cast<const char *>(vertices.data()), vertices.size());
    while (bin.size() % 4 != 0) bin += '\0';
    const auto index_offset = bin.size();
    bin.append(reinterpret_cast<const char *>(triangles_data.data()), triangles_data.size());

    // Buffer 1 is the uncompressed fallback, absent from the file.
    const auto json = [&](std::size_t vertex_bytes) {
        return std::format(
            R"("extensionsUsed":["EXT_meshopt_compression"],)"
            R"("extensionsRequired":["EXT_meshopt_compression"],)"
            R"("buffers":[{{"byteLength":{}}},)"
            R"({{"byteLength":72,"extensions":{{"EXT_meshopt_compression":{{"fallback":true}}}}}}],)"
            R"("bufferViews":[)"
            R"({{"buffer":1,"byteLength":48,"byteStride":12,"extensions":{{"EXT_meshopt_compression":)"
            R"({{"buffer":0,"byteLength":{},"byteStride":12,"count":4,"mode":"ATTRIBUTES"}}}}}},)"
            R"({{"buffer":1,"byteOffset":48,"byteLength":24,"extensions":{{"EXT_meshopt_compression":)"
            R"({{"buffer":0,"byteOffset":{},"byteLength":{},"byteStride":4,"count":6,"mode":"TRIANGLES"}}}}}}],)"
            R"("accessors":[{{"bufferView":0,"componentType":5126,"type":"VEC3","count":4}},)"
            R"({{"bufferView":1,"componentType":5125,"type":"SCALAR","count":6}}],)"
            R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":0}},"indices":1}}]}}])",
            bin.size(), vertex_bytes, index_offset, triangles_data.size());
    };

    const auto doc = load_glb(dir, json(vertices.size()), bin);
    REQUIRE(doc);
    const auto geometry = gltf::to_mesh_geometry(*doc, 0, 0);
    REQUIRE(geometry);
    using Tri = std::array<std::uint32_t, 3>;
    CHECK(triangles(*geometry) == std::vector<Tri>{{0, 1, 2}, {0, 2, 3}});
    const auto p = *geometry->attribute_for(sg::semantics::position())->view<float, 3>();
    for (std::size_t v = 0; v < 4; ++v) {
        for (std::size_t c = 0; c < 3; ++c) CHECK(p[c, v] == positions[3 * v + c]);
    }

    // Truncated compressed data is reported.
    const auto broken = load_glb(dir, json(vertices.size() / 2), bin);
    REQUIRE_FALSE(broken);
    CHECK_THAT(broken.error().message, ContainsSubstring("ATTRIBUTES data is corrupt"));

    // A malformed extension is reported, not defaulted.
    auto bad_mode = json(vertices.size());
    bad_mode.replace(bad_mode.find("ATTRIBUTES"), std::string_view("ATTRIBUTES").size(), "VERTICES");
    const auto malformed = load_glb(dir, bad_mode, bin);
    REQUIRE_FALSE(malformed);
    CHECK_THAT(malformed.error().message,
               ContainsSubstring("invalid EXT_meshopt_compression"));
}
#endif

#if defined(BALSA_VISUALIZATION_DRACO)
#include <draco/compression/decode.h>
#include <draco/compression/encode.h>
#include <draco/mesh/triangle_soup_mesh_builder.h>

TEST_CASE("gltf_draco_compression", "[gltf]") {
    TempDir dir;
    namespace sg = balsa::scene_graph;
    using Point = std::array<float, 3>;
    // A quad as two triangles.
    const std::array<std::array<Point, 3>, 2> quad{{
        {{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}}},
        {{{0, 0, 0}, {1, 1, 0}, {0, 1, 0}}},
    }};

    draco::TriangleSoupMeshBuilder builder;
    builder.Start(2);
    const int position = builder.AddAttribute(draco::GeometryAttribute::POSITION, 3,
                                              draco::DT_FLOAT32);
    for (int f = 0; f < 2; ++f) {
        builder.SetAttributeValuesForFace(position, draco::FaceIndex(f), quad[f][0].data(),
                                          quad[f][1].data(), quad[f][2].data());
    }
    const auto mesh = builder.Finalize();
    REQUIRE(mesh);
    draco::EncoderBuffer encoded;
    draco::Encoder encoder;
    REQUIRE(encoder.EncodeMeshToBuffer(*mesh, &encoded).ok());
    const std::string bin(encoded.data(), encoded.size());

    // The accessor counts must match the decoded mesh.
    draco::DecoderBuffer buffer;
    buffer.Init(encoded.data(), encoded.size());
    const auto decoded = draco::Decoder().DecodeMeshFromBuffer(&buffer);
    REQUIRE(decoded.ok());
    const auto points = decoded.value()->num_points();
    const auto id = decoded.value()->GetNamedAttribute(draco::GeometryAttribute::POSITION)
                        ->unique_id();

    const auto doc = load_glb(
        dir,
        std::format(R"("extensionsUsed":["KHR_draco_mesh_compression"],)"
                    R"("extensionsRequired":["KHR_draco_mesh_compression"],)"
                    R"("buffers":[{{"byteLength":{}}}],)"
                    R"("bufferViews":[{{"buffer":0,"byteLength":{}}}],)"
                    R"("accessors":[{{"componentType":5126,"type":"VEC3","count":{}}},)"
                    R"({{"componentType":5125,"type":"SCALAR","count":6}}],)"
                    R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":0}},"indices":1,)"
                    R"("extensions":{{"KHR_draco_mesh_compression":{{"bufferView":0,)"
                    R"("attributes":{{"POSITION":{}}}}}}}}}]}}])",
                    bin.size(), bin.size(), points, id),
        bin);
    REQUIRE(doc);
    const auto geometry = gltf::to_mesh_geometry(*doc, 0, 0);
    REQUIRE(geometry);
    REQUIRE(geometry->triangle_count() == 2);

    // Each triangle's corners, rotated to start at the smallest, match.
    const auto p = *geometry->attribute_for(sg::semantics::position())->view<float, 3>();
    std::vector<std::array<Point, 3>> found;
    for (const auto &[a, b, c] : triangles(*geometry)) {
        std::array<Point, 3> corners{};
        for (const auto &[k, v] : std::views::enumerate(std::array{a, b, c})) {
            corners[static_cast<std::size_t>(k)] = {p[0, v], p[1, v], p[2, v]};
        }
        std::ranges::rotate(corners, std::ranges::min_element(corners));
        found.push_back(corners);
    }
    auto expected = std::vector(quad.begin(), quad.end());
    for (auto &corners : expected) std::ranges::rotate(corners, std::ranges::min_element(corners));
    std::ranges::sort(found);
    std::ranges::sort(expected);
    CHECK(found == expected);
}
#endif

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {
// A 2 x 1 RGB PNG: red, then green.
auto two_pixel_png() -> std::string {
    const std::array<uint8_t, 6> pixels{255, 0, 0, 0, 255, 0};
    std::string png;
    stbi_write_png_to_func(
        [](void *context, void *data, int size) {
            static_cast<std::string *>(context)->append(static_cast<const char *>(data),
                                                        static_cast<std::size_t>(size));
        },
        &png, 2, 1, 3, pixels.data(), 6);
    return png;
}
} // namespace

TEST_CASE("gltf_materials", "[gltf][material]") {
    namespace sg = balsa::scene_graph;
    TempDir dir;
    dir.write("texture.png", two_pixel_png());
    const auto load_materials = [&](std::string_view json) {
        auto document = gltf::load(dir.write(
            "materials.gltf", std::format(R"({{"asset":{{"version":"2.0"}},{}}})", json)));
        REQUIRE(document);
        return gltf::to_materials(*document);
    };

    const auto materials = load_materials(R"(
        "images": [{"uri": "texture.png"}],
        "samplers": [{"magFilter": 9728, "minFilter": 9986, "wrapS": 33071}],
        "textures": [{"source": 0, "sampler": 0, "name": "sampled"}, {"source": 0}],
        "materials": [
            {"name": "full",
             "pbrMetallicRoughness": {
                 "baseColorFactor": [0.5, 0.25, 1, 0.75], "metallicFactor": 0.2,
                 "baseColorTexture": {"index": 0, "texCoord": 1},
                 "metallicRoughnessTexture": {"index": 1}},
             "normalTexture": {"index": 0, "scale": 2},
             "occlusionTexture": {"index": 1, "strength": 0.5},
             "emissiveFactor": [1, 0, 0],
             "alphaMode": "MASK", "alphaCutoff": 0.3, "doubleSided": true},
            {"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
            {}
        ])");
    REQUIRE(materials);
    REQUIRE(materials->size() == 3);

    const auto &full = *(*materials)[0];
    CHECK(full.name == "full");
    CHECK(full.base_color(1) == 0.25f);
    CHECK(full.base_color(3) == 0.75f);
    CHECK(full.metallic == 0.2f);
    CHECK(full.roughness == 1.0f);
    CHECK(full.normal_scale == 2.0f);
    CHECK(full.occlusion_strength == 0.5f);
    CHECK(full.emissive(0) == 1.0f);
    CHECK(full.alpha_mode == sg::AlphaMode::Mask);
    CHECK(full.alpha_cutoff == 0.3f);
    CHECK(full.double_sided);
    CHECK_FALSE(full.emissive_texture);

    // Texture 0: the decoded image and its sampler.
    REQUIRE(full.base_color_texture);
    CHECK(full.base_color_texture->set == 1);
    const auto &sampled = *full.base_color_texture->texture;
    CHECK(sampled.name == "sampled");
    REQUIRE(sampled.image);
    CHECK(sampled.image->width() == 2);
    CHECK(sampled.image->channels() == 3);
    CHECK((*sampled.image->view<uint8_t>())[0, 1, 1] == 255);
    CHECK(sampled.sampler == sg::Sampler{.magnification = sg::TextureFilter::Nearest,
                                         .minification = sg::TextureFilter::Nearest,
                                         .mipmap = sg::TextureFilter::Linear,
                                         .wrap_u = sg::TextureWrap::ClampToEdge,
                                         .wrap_v = sg::TextureWrap::Repeat});

    // Textures and images are shared, not converted again.
    CHECK(full.normal_texture->texture == full.base_color_texture->texture);
    CHECK((*materials)[1]->base_color_texture->texture == full.base_color_texture->texture);
    const auto &plain = *full.metallic_roughness_texture->texture;
    CHECK(&plain != &sampled);
    CHECK(plain.image == sampled.image);
    CHECK(plain.sampler == sg::Sampler{});

    // A material without properties has glTF's defaults.
    const auto &empty = *(*materials)[2];
    CHECK(empty.base_color(0) == 1.0f);
    CHECK(empty.alpha_mode == sg::AlphaMode::Opaque);
    CHECK_FALSE(empty.base_color_texture);

    SECTION("errors") {
        const auto missing = load_materials(R"(
            "materials": [{"normalTexture": {"index": 4}}])");
        REQUIRE_FALSE(missing);
        CHECK_THAT(missing.error().message, ContainsSubstring("material 0: no texture 4"));

        dir.write("broken.png", "not a png");
        const auto broken = load_materials(R"(
            "images": [{"uri": "broken.png"}],
            "textures": [{"source": 0}],
            "materials": [{"emissiveTexture": {"index": 0}}])");
        REQUIRE_FALSE(broken);
        CHECK_THAT(broken.error().message, ContainsSubstring("image 0"));
    }
}
