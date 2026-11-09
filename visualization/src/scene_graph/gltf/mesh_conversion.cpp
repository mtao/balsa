#include "balsa/visualization/scene_graph/gltf/mesh_conversion.hpp"

#include "balsa/visualization/scene_graph/gltf/detail/LoadErrors.hpp"

#include <balsa/scene_graph/AttributeSemantic.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <ranges>
#include <type_traits>

namespace balsa::visualization::gltf {

namespace {
    namespace sg = balsa::scene_graph;
    using Indices = std::vector<std::uint32_t>;

    auto invalid(std::string message) -> std::unexpected<LoadError> {
        return std::unexpected(
            LoadError{LoadError::Code::InvalidGltf, std::move(message)});
    }

    // Adds `view` as the vertex attribute for glTF `semantic`, keeping its
    // type and normalization and tagging its balsa semantic.
    auto add_attribute(sg::MeshGeometry &geometry,
                       std::string_view semantic,
                       const AccessorView &view) -> bool {
        sg::AttributeOptions options{.normalized = view.normalized};
        if (const auto tag = attribute_semantic(semantic)) {
            options.semantic = tag->semantic;
            options.set = tag->set;
        }
        return std::visit(
            [&](const auto &values) -> bool {
                return bool(geometry.add_attribute(attribute_name(semantic),
                                                   sg::AttributeDomain::Vertex,
                                                   values,
                                                   options));
            },
            view.data);
    }

    // The primitive's indices, or 0..vertex_count-1 without them.
    auto vertex_sequence(const Document &document,
                         const MeshPrimitive &primitive,
                         std::size_t vertex_count) -> Indices {
        if (!primitive.indices) {
            return std::views::iota(std::uint32_t{0},
                                    static_cast<std::uint32_t>(vertex_count))
                   | std::ranges::to<Indices>();
        }
        const auto view =
            document.accessor(static_cast<std::size_t>(*primitive.indices));
        return std::visit(
            [](const auto &indices) {
                return std::views::iota(std::size_t{0}, indices.extent(2))
                       | std::views::transform([&](std::size_t i) {
                             return static_cast<std::uint32_t>(indices[0, 0, i]);
                         })
                       | std::ranges::to<Indices>();
            },
            view.data);
    }

    // make(e) gives element e's indices.
    template <typename Make>
    auto elements(std::size_t count, Make make) -> Indices {
        return std::views::iota(std::size_t{0}, count)
               | std::views::transform(make) | std::views::join
               | std::ranges::to<Indices>();
    }

    // (elements, K): set_*_indices read a K x K view as rows, so
    // (K, elements) would be transposed when there are exactly K elements.
    template <std::size_t K>
    auto as_elements(const Indices &indices) {
        return ::zipper::mdspan<const std::uint32_t,
                                ::zipper::dextents<2>,
                                ::zipper::storage::row_major>(
            indices.data(), indices.size() / K, K);
    }

    // Triangles or edges for `mode`, per glTF's topology rules.
    auto topology(PrimitiveMode mode, const Indices &s)
        -> std::expected<Indices, std::string> {
        using V = std::uint32_t;
        const auto n = s.size();
        switch (mode) {
        case PrimitiveMode::POINTS: return Indices{};
        case PrimitiveMode::TRIANGLES:
            if (n % 3 != 0) {
                return std::unexpected(
                    std::format("has {} TRIANGLES indices, not a multiple of 3",
                                n));
            }
            return s;
        case PrimitiveMode::TRIANGLE_STRIP:
            // p_i = {v_i, v_{i + (1 + i % 2)}, v_{i + (2 - i % 2)}}
            return elements(n < 3 ? 0 : n - 2, [&](std::size_t i) {
                return std::array<V, 3>{
                    s[i], s[i + 1 + i % 2], s[i + 2 - i % 2]};
            });
        case PrimitiveMode::TRIANGLE_FAN:
            // p_i = {v_{i + 1}, v_{i + 2}, v_0}
            return elements(n < 3 ? 0 : n - 2, [&](std::size_t i) {
                return std::array<V, 3>{s[i + 1], s[i + 2], s[0]};
            });
        case PrimitiveMode::LINES:
            if (n % 2 != 0) {
                return std::unexpected(
                    std::format("has {} LINES indices, not a multiple of 2",
                                n));
            }
            return s;
        case PrimitiveMode::LINE_STRIP:
            return elements(n < 2 ? 0 : n - 1, [&](std::size_t i) {
                return std::array<V, 2>{s[i], s[i + 1]};
            });
        case PrimitiveMode::LINE_LOOP:
            return elements(n < 2 ? 0 : n, [&](std::size_t i) {
                return std::array<V, 2>{s[i], s[(i + 1) % n]};
            });
        }
        return std::unexpected(std::string("has an unknown mode"));
    }

    auto is_triangles(PrimitiveMode mode) -> bool {
        return mode == PrimitiveMode::TRIANGLES
               || mode == PrimitiveMode::TRIANGLE_STRIP
               || mode == PrimitiveMode::TRIANGLE_FAN;
    }
} // namespace

auto attribute_name(std::string_view semantic) -> std::string {
    if (semantic == "POSITION") { return "vertex_positions"; }
    if (semantic == "NORMAL") { return "vertex_normals"; }
    return std::string(semantic);
}

auto attribute_semantic(std::string_view semantic)
    -> std::optional<AttributeSemanticSet> {
    if (semantic == "POSITION") { return {{sg::semantics::position()}}; }
    if (semantic == "NORMAL") { return {{sg::semantics::normal()}}; }
    if (semantic == "TANGENT") { return {{sg::semantics::tangent()}}; }
    // PREFIX_n, n a set index.
    const auto indexed = [&](std::string_view prefix) -> std::optional<std::uint8_t> {
        if (!semantic.starts_with(prefix)) { return std::nullopt; }
        const auto digits = semantic.substr(prefix.size());
        std::uint8_t set = 0;
        const auto [end, error] =
            std::from_chars(digits.data(), digits.data() + digits.size(), set);
        if (digits.empty() || error != std::errc{}
            || end != digits.data() + digits.size()) {
            return std::nullopt;
        }
        return set;
    };
    if (const auto set = indexed("TEXCOORD_")) {
        return {{sg::semantics::texture_coordinates(), *set}};
    }
    if (const auto set = indexed("COLOR_")) { return {{sg::semantics::color(), *set}}; }
    return std::nullopt;
}

auto to_mesh_geometry(const Document &document,
                      std::size_t mesh,
                      std::size_t primitive)
    -> std::expected<sg::MeshGeometry, LoadError> {
    const auto &meshes = document.manifest().meshes;
    if (mesh >= meshes.size()) { return invalid(std::format("no mesh {}", mesh)); }
    if (primitive >= meshes[mesh].primitives.size()) {
        return invalid(
            std::format("mesh {} has no primitive {}", mesh, primitive));
    }
    const auto &entry = meshes[mesh].primitives[primitive];
    const auto context = [&](std::string_view message) {
        return std::format("mesh {} primitive {} {}", mesh, primitive, message);
    };

    sg::MeshGeometry geometry;
    // load() checked that every attribute has the same count.
    std::size_t vertex_count = 0;
    for (const auto &[semantic, index] : entry.attributes) {
        const auto view = document.accessor(static_cast<std::size_t>(index));
        vertex_count = view.count();
        if (!add_attribute(geometry, semantic, view)) {
            return invalid(context(
                std::format("cannot add attribute {} as '{}'",
                            semantic,
                            attribute_name(semantic))));
        }
    }

    const auto sequence = vertex_sequence(document, entry, vertex_count);
    if (const auto out_of_range = std::ranges::find_if(
            sequence, [&](std::uint32_t v) { return v >= vertex_count; });
        out_of_range != sequence.end()) {
        return invalid(context(std::format("has index {} but {} vertices",
                                           *out_of_range,
                                           vertex_count)));
    }
    const auto indices = topology(entry.mode, sequence);
    if (!indices) { return invalid(context(indices.error())); }
    if (!indices->empty()) {
        if (is_triangles(entry.mode)) {
            geometry.set_triangle_indices(as_elements<3>(*indices));
        } else {
            geometry.set_edge_indices(as_elements<2>(*indices));
        }
    }
    return geometry;
}

auto to_mesh_geometries(const Document &document, std::size_t mesh)
    -> std::expected<std::vector<MeshPrimitiveGeometry>, LoadError> {
    const auto &meshes = document.manifest().meshes;
    if (mesh >= meshes.size()) { return invalid(std::format("no mesh {}", mesh)); }

    detail::LoadErrors errors;
    std::vector<MeshPrimitiveGeometry> primitives;
    for (const auto &[p, entry] : std::views::enumerate(meshes[mesh].primitives)) {
        auto geometry =
            to_mesh_geometry(document, mesh, static_cast<std::size_t>(p));
        if (!geometry) {
            errors.add(std::move(geometry.error()));
            continue;
        }
        primitives.push_back(MeshPrimitiveGeometry{
            .geometry = std::move(*geometry),
            .mode = entry.mode,
            .material = entry.material.transform(
                [](std::int64_t m) { return static_cast<std::size_t>(m); }),
        });
    }
    if (!errors.empty()) { return std::unexpected(std::move(errors).combined()); }
    return primitives;
}

} // namespace balsa::visualization::gltf
