#include "balsa/visualization/scene_graph/gltf/detail/draco.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/accessors.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/extension.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/little_endian.hpp"

#include <draco/compression/decode.h>
#include <draco/mesh/mesh.h>

#include <array>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <string>

namespace balsa::visualization::scene_graph::gltf::detail {

namespace {
    // A primitive's Draco extension: its compressed data and the Draco
    // attribute id of each compressed glTF attribute.
    struct [[=object]] DracoPrimitive {
        [[=json::required]] std::size_t bufferView;
        [[=json::required]] std::map<std::string, uint32_t> attributes;
    };
    auto from_json(const nlohmann::json &j, DracoPrimitive &v) -> void {
        json::from_json_members(j, v);
    }

    // Calls f.template operator()<T>() for the accessor's component type.
    template <typename F>
    auto with_component_type(ComponentType type, F &&f) {
        switch (type) {
        case ComponentType::BYTE: return f.template operator()<std::int8_t>();
        case ComponentType::UNSIGNED_BYTE: return f.template operator()<std::uint8_t>();
        case ComponentType::SHORT: return f.template operator()<std::int16_t>();
        case ComponentType::UNSIGNED_SHORT: return f.template operator()<std::uint16_t>();
        case ComponentType::UNSIGNED_INT: return f.template operator()<std::uint32_t>();
        case ComponentType::FLOAT: break;
        }
        return f.template operator()<float>();
    }

    // The mesh's triangle indices as the accessor's values.
    auto write_indices(const draco::Mesh &mesh, const Accessor &accessor,
                       std::vector<std::byte> &values) -> std::optional<std::string> {
        const auto layout = accessor_layout(accessor);
        const auto count = static_cast<std::size_t>(accessor.count);
        if (count != 3 * mesh.num_faces()) {
            return std::format("{} indices for {} triangles", count, mesh.num_faces());
        }
        values.assign(count * layout.element_size, std::byte{0});
        with_component_type(accessor.componentType, [&]<typename T>() {
            for (draco::FaceIndex f(0); f < mesh.num_faces(); ++f) {
                for (std::size_t k = 0; k < 3; ++k) {
                    write_little_endian(std::span(values).subspan((3 * f.value() + k) * layout.element_size),
                             static_cast<T>(mesh.face(f)[k].value()));
                }
            }
        });
        return std::nullopt;
    }

    // Draco attribute `attribute`, per point, as the accessor's values.
    auto write_attribute(const draco::Mesh &mesh, const draco::PointAttribute &attribute,
                         const Accessor &accessor, std::vector<std::byte> &values)
        -> std::optional<std::string> {
        const auto layout = accessor_layout(accessor);
        const auto count = static_cast<std::size_t>(accessor.count);
        if (count != mesh.num_points()) {
            return std::format("{} values for {} points", count, mesh.num_points());
        }
        if (layout.columns != 1) return std::string("matrix attributes are not supported");
        values.assign(count * layout.element_size, std::byte{0});
        const auto components = static_cast<int8_t>(layout.rows);
        bool converted = true;
        with_component_type(accessor.componentType, [&]<typename T>() {
            std::array<T, 4> point{};
            for (draco::PointIndex p(0); p < mesh.num_points(); ++p) {
                converted = converted
                            && attribute.ConvertValue<T>(attribute.mapped_index(p),
                                                         components, point.data());
                for (std::size_t c = 0; c < layout.rows; ++c) {
                    write_little_endian(std::span(values).subspan(p.value() * layout.element_size
                                                       + c * layout.component_size),
                             point[c]);
                }
            }
        });
        if (!converted) return std::string("values cannot be converted");
        return std::nullopt;
    }
} // namespace

auto decode_draco_accessors(const glTF &manifest,
                            const std::vector<std::span<const std::byte>> &buffer_views,
                            LoadErrors &errors)
    -> std::vector<std::optional<std::vector<std::byte>>> {
    std::vector<std::optional<std::vector<std::byte>>> accessors(manifest.accessors.size());
    for (const auto &[m, mesh] : std::views::enumerate(manifest.meshes)) {
        for (const auto &[p, primitive] : std::views::enumerate(mesh.primitives)) {
            const auto fail = [&](std::string_view message) {
                errors.add(LoadError::Code::InvalidGltf,
                           std::format("mesh {} primitive {}: Draco {}", m, p, message));
            };
            const std::array names{draco_extension};
            const auto parsed = parse_extension<DracoPrimitive>(primitive.extensions, names);
            if (!parsed) {
                fail(parsed.error());
                continue;
            }
            if (!*parsed) continue;
            const auto &ext = **parsed;

            const auto view = ext.bufferView;
            if (view >= buffer_views.size()) {
                fail("bufferView is missing");
                continue;
            }
            draco::DecoderBuffer buffer;
            buffer.Init(reinterpret_cast<const char *>(buffer_views[view].data()),
                        buffer_views[view].size());
            draco::Decoder decoder;
            auto decoded = decoder.DecodeMeshFromBuffer(&buffer);
            if (!decoded.ok()) {
                fail(std::format("data is invalid: {}", decoded.status().error_msg_string()));
                continue;
            }
            const auto &draco_mesh = *decoded.value();

            if (primitive.indices) {
                const auto index = static_cast<std::size_t>(*primitive.indices);
                std::vector<std::byte> values;
                if (const auto problem =
                        write_indices(draco_mesh, manifest.accessors[index], values)) {
                    fail(*problem);
                } else {
                    accessors[index] = std::move(values);
                }
            }
            for (const auto &[semantic, accessor] : primitive.attributes) {
                const auto id = ext.attributes.find(semantic);
                if (id == ext.attributes.end()) continue; // stored uncompressed
                const auto *attribute = draco_mesh.GetAttributeByUniqueId(id->second);
                if (attribute == nullptr) {
                    fail(std::format("attribute {} is missing", semantic));
                    continue;
                }
                const auto index = static_cast<std::size_t>(accessor);
                std::vector<std::byte> values;
                if (const auto problem = write_attribute(draco_mesh, *attribute,
                                                         manifest.accessors[index], values)) {
                    fail(std::format("attribute {}: {}", semantic, *problem));
                } else {
                    accessors[index] = std::move(values);
                }
            }
        }
    }
    return accessors;
}

} // namespace balsa::visualization::scene_graph::gltf::detail
