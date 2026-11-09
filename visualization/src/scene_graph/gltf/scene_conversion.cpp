#include "balsa/visualization/scene_graph/gltf/scene_conversion.hpp"

#include "balsa/visualization/scene_graph/gltf/detail/LoadErrors.hpp"
#include "balsa/visualization/scene_graph/gltf/material_conversion.hpp"
#include "balsa/visualization/scene_graph/gltf/mesh_conversion.hpp"
#include <balsa/scene_graph/BasicShading.hpp>
#include <balsa/scene_graph/MeshRenderer.hpp>

#include <balsa/scene_graph/MeshInstance.hpp>

#include <zipper/MatrixBase.hpp>

#include <cstdint>
#include <format>
#include <ranges>
#include <vector>

namespace balsa::visualization::gltf {

namespace {
    namespace sg = balsa::scene_graph;
    // A converted primitive and its material.
    struct Primitive {
        std::shared_ptr<sg::MeshGeometry> geometry;
        std::shared_ptr<sg::Material> material;
    };
    using Primitives = std::vector<Primitive>;

    // A primitive's MeshInstance and shading: triangles are drawn with
    // their material (wireframe and points off); lines and points keep the
    // default layers.
    void show(sg::Object &object, const Primitive &primitive) {
        object.emplace_feature<sg::MeshInstance>(primitive.geometry);
        const auto dimension = primitive.geometry->dimension();
        auto layers = sg::make_default_mesh_render_layers(dimension);
        if (dimension >= 2) {
            for (auto &layer : layers) {
                if (layer.renderer && layer.renderer->id() == "balsa.phong_surface") {
                    layer.renderer = std::make_unique<sg::PbrSurfaceRenderer>(primitive.material);
                } else {
                    layer.enabled = false;
                }
            }
        }
        object.emplace_feature<sg::BasicShading>(std::move(layers)).fit_to_geometry();
    }

    auto is_identity(const std::array<double, 16> &matrix) -> bool {
        return std::ranges::equal(
            matrix,
            std::array<double, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    }

    // The node's matrix if set (glTF forbids it alongside TRS), else TRS.
    void set_transform(sg::Object &object, const Node &node) {
        if (!is_identity(node.matrix)) {
            const ::zipper::MatrixBase matrix(
                ::zipper::mdspan<const double,
                                 ::zipper::extents<4, 4>,
                                 ::zipper::storage::col_major>(node.matrix.data()));
            object.set_from_transform(sg::AffineTransformf(matrix));
            return;
        }
        const auto &[tx, ty, tz] = node.translation;
        const auto &[qx, qy, qz, qw] = node.rotation;
        const auto &[sx, sy, sz] = node.scale;
        object.set_translation(sg::Vec3f{float(tx), float(ty), float(tz)});
        object.set_rotation(
            sg::Quaternionf(float(qw), float(qx), float(qy), float(qz)));
        object.set_scale_factors(sg::Vec3f{float(sx), float(sy), float(sz)});
    }

    // Builds node trees, converting each mesh once.
    class SceneBuilder {
      public:
        SceneBuilder(const Document &document,
                     std::vector<std::shared_ptr<sg::Material>> materials)
          : m_document(document),
            m_materials(std::move(materials)),
            m_meshes(document.manifest().meshes.size()) {}

        void add_node(sg::Object &parent, std::size_t index) {
            const auto &node = m_document.manifest().nodes[index];
            auto &object = parent.add_child(
                node.name.value_or(std::format("node {}", index)));
            set_transform(object, node);

            if (node.mesh) {
                const auto &primitives =
                    mesh(static_cast<std::size_t>(*node.mesh));
                if (primitives.size() == 1) {
                    show(object, primitives.front());
                } else {
                    for (const auto &[k, primitive] : std::views::enumerate(primitives)) {
                        show(object.add_child(std::format("primitive {}", k)), primitive);
                    }
                }
            }
            for (const auto child : node.children) {
                add_node(object, static_cast<std::size_t>(child));
            }
        }

        auto errors() && -> detail::LoadErrors { return std::move(m_errors); }

      private:
        auto mesh(std::size_t index) -> const Primitives & {
            auto &cached = m_meshes[index];
            if (!cached) {
                cached.emplace();
                if (auto primitives = to_mesh_geometries(m_document, index)) {
                    for (auto &primitive : *primitives) {
                        cached->push_back(
                            {std::make_shared<sg::MeshGeometry>(std::move(primitive.geometry)),
                             material(primitive.material)});
                    }
                } else {
                    m_errors.add(std::move(primitives.error()));
                }
            }
            return *cached;
        }

        // glTF's default material when a primitive has none.
        auto material(std::optional<std::size_t> index) -> std::shared_ptr<sg::Material> {
            if (index && *index < m_materials.size()) return m_materials[*index];
            if (!m_default_material) m_default_material = std::make_shared<sg::Material>();
            return m_default_material;
        }

        const Document &m_document;
        std::vector<std::shared_ptr<sg::Material>> m_materials;
        std::shared_ptr<sg::Material> m_default_material;
        std::vector<std::optional<Primitives>> m_meshes;
        detail::LoadErrors m_errors;
    };

    auto root_nodes(const glTF &manifest) -> std::vector<std::size_t> {
        std::vector<bool> is_child(manifest.nodes.size(), false);
        for (const auto &node : manifest.nodes) {
            for (const auto child : node.children) {
                is_child[static_cast<std::size_t>(child)] = true;
            }
        }
        return std::views::iota(std::size_t{0}, manifest.nodes.size())
               | std::views::filter([&](std::size_t n) { return !is_child[n]; })
               | std::ranges::to<std::vector>();
    }
} // namespace

auto to_object(const Document &document, std::optional<std::size_t> scene)
    -> std::expected<std::unique_ptr<sg::Object>, LoadError> {
    const auto &manifest = document.manifest();
    if (!scene && manifest.scene) {
        scene = static_cast<std::size_t>(*manifest.scene);
    } else if (!scene && !manifest.scenes.empty()) {
        scene = 0;
    }
    if (scene && *scene >= manifest.scenes.size()) {
        return std::unexpected(LoadError{LoadError::Code::InvalidGltf,
                                         std::format("no scene {}", *scene)});
    }

    // load() checked that scene nodes are existing root nodes.
    auto root = std::make_unique<sg::Object>(
        scene ? manifest.scenes[*scene].name.value_or(
                    std::format("scene {}", *scene))
              : std::string("scene"));
    const auto nodes =
        scene ? manifest.scenes[*scene].nodes
                    | std::views::transform([](std::int64_t n) {
                          return static_cast<std::size_t>(n);
                      })
                    | std::ranges::to<std::vector>()
              : root_nodes(manifest);

    auto materials = to_materials(document);
    if (!materials) return std::unexpected(std::move(materials.error()));
    SceneBuilder builder(document, std::move(*materials));
    for (const auto node : nodes) { builder.add_node(*root, node); }
    if (auto errors = std::move(builder).errors(); !errors.empty()) {
        return std::unexpected(std::move(errors).combined());
    }
    return root;
}

} // namespace balsa::visualization::gltf
