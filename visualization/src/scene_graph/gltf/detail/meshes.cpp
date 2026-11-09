#include "balsa/visualization/scene_graph/gltf/detail/meshes.hpp"

#include <cstddef>
#include <format>
#include <optional>
#include <ranges>

namespace balsa::visualization::gltf::detail {

void check_meshes(const glTF &manifest, LoadErrors &errors) {
    const auto accessor = [&](std::int64_t index) -> const Accessor * {
        if (index < 0
            || static_cast<std::size_t>(index) >= manifest.accessors.size()) {
            return nullptr;
        }
        return &manifest.accessors[static_cast<std::size_t>(index)];
    };

    for (const auto &[m, mesh] : std::views::enumerate(manifest.meshes)) {
        for (const auto &[p, primitive] : std::views::enumerate(mesh.primitives)) {
            const auto fail = [&](std::string message) {
                errors.add(LoadError::Code::InvalidGltf,
                           std::format("mesh {} primitive {} {}", m, p, message));
            };

            std::optional<std::int64_t> vertex_count;
            for (const auto &[semantic, index] : primitive.attributes) {
                const auto *values = accessor(index);
                if (values == nullptr) {
                    fail(std::format("attribute {} refers to missing "
                                     "accessor {}",
                                     semantic,
                                     index));
                    continue;
                }
                if (!vertex_count) {
                    vertex_count = values->count;
                } else if (values->count != *vertex_count) {
                    fail(std::format("attribute {} has {} values, but other "
                                     "attributes have {}",
                                     semantic,
                                     values->count,
                                     *vertex_count));
                }
            }

            if (primitive.indices) {
                const auto *indices = accessor(*primitive.indices);
                if (indices == nullptr) {
                    fail(std::format("refers to missing indices accessor {}",
                                     *primitive.indices));
                } else if (indices->type != AccessorType::SCALAR
                           || indices->normalized
                           || (indices->componentType
                                   != ComponentType::UNSIGNED_BYTE
                               && indices->componentType
                                      != ComponentType::UNSIGNED_SHORT
                               && indices->componentType
                                      != ComponentType::UNSIGNED_INT)) {
                    fail(std::format("has indices accessor {} that is not "
                                     "an unsigned integer scalar",
                                     *primitive.indices));
                }
            }

            if (primitive.material
                && (*primitive.material < 0
                    || static_cast<std::size_t>(*primitive.material)
                           >= manifest.materials.size())) {
                fail(std::format("refers to missing material {}",
                                 *primitive.material));
            }

            for (const auto &[t, target] : std::views::enumerate(primitive.targets)) {
                for (const auto &[semantic, index] : target) {
                    if (accessor(index) == nullptr) {
                        fail(std::format("morph target {} attribute {} refers "
                                         "to missing accessor {}",
                                         t,
                                         semantic,
                                         index));
                    }
                }
            }
        }
    }
}

} // namespace balsa::visualization::gltf::detail
