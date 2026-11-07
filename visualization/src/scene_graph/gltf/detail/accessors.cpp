#include "balsa/visualization/scene_graph/gltf/detail/accessors.hpp"
#include "balsa/visualization/scene_graph/gltf/AccessorView.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>

namespace balsa::visualization::scene_graph::gltf::detail {

namespace {
    auto rows_and_columns(AccessorType type)
        -> std::pair<std::size_t, std::size_t> {
        switch (type) {
        case AccessorType::SCALAR: return {1, 1};
        case AccessorType::VEC2: return {2, 1};
        case AccessorType::VEC3: return {3, 1};
        case AccessorType::VEC4: return {4, 1};
        case AccessorType::MAT2: return {2, 2};
        case AccessorType::MAT3: return {3, 3};
        case AccessorType::MAT4: return {4, 4};
        }
        return {1, 1};
    }

    auto is_matrix(AccessorType type) -> bool {
        return type == AccessorType::MAT2 || type == AccessorType::MAT3
               || type == AccessorType::MAT4;
    }

    // `count` little-endian T values, tightly packed at the start of `bytes`.
    template <typename T>
    auto packed_values(std::span<const std::byte> bytes, std::size_t count) {
        return ::zipper::mdspan<const T,
                                ::zipper::dextents<1>,
                                ::zipper::default_layout_policy,
                                LittleEndianAccessor<T>>(bytes.data(), count);
    }

    auto find_view(const glTF &manifest, std::int64_t index)
        -> const BufferView * {
        if (index < 0
            || static_cast<std::size_t>(index) >= manifest.bufferViews.size()) {
            return nullptr;
        }
        return &manifest.bufferViews[static_cast<std::size_t>(index)];
    }
} // namespace

auto component_size(ComponentType type) -> std::size_t {
    switch (type) {
    case ComponentType::BYTE:
    case ComponentType::UNSIGNED_BYTE: return 1;
    case ComponentType::SHORT:
    case ComponentType::UNSIGNED_SHORT: return 2;
    case ComponentType::UNSIGNED_INT:
    case ComponentType::FLOAT: return 4;
    }
    return 1;
}

auto accessor_layout(const Accessor &accessor) -> AccessorLayout {
    const auto size = component_size(accessor.componentType);
    const auto [rows, columns] = rows_and_columns(accessor.type);
    auto column_stride = rows * size;
    if (is_matrix(accessor.type)) {
        column_stride = (column_stride + 3) / 4 * 4;
    }
    return AccessorLayout{.component_size = size,
                          .rows = rows,
                          .columns = columns,
                          .column_stride = column_stride,
                          .element_size = column_stride * columns};
}

void check_accessors(const glTF &manifest, LoadErrors &errors) {
    for (const auto &[i, accessor] : std::views::enumerate(manifest.accessors)) {
        const auto fail = [&](std::string message) {
            errors.add(LoadError::Code::InvalidGltf,
                       std::format("accessor {} {}", i, message));
        };
        const auto layout = accessor_layout(accessor);
        if (accessor.count < 1) {
            fail(std::format("has count {}", accessor.count));
            continue;
        }
        const auto count = static_cast<std::size_t>(accessor.count);
        if (accessor.normalized
            && (accessor.componentType == ComponentType::FLOAT
                || accessor.componentType == ComponentType::UNSIGNED_INT)) {
            fail("is normalized but its components are not 8 or 16 bit");
        }

        if (accessor.bufferView) {
            const auto *view = find_view(manifest, *accessor.bufferView);
            if (view == nullptr) {
                fail(std::format("refers to missing bufferView {}",
                                 *accessor.bufferView));
                continue;
            }
            const auto offset = static_cast<std::size_t>(accessor.byteOffset);
            if ((static_cast<std::size_t>(view->byteOffset) + offset)
                    % layout.component_size
                != 0) {
                fail(std::format("is not aligned to its {}-byte components",
                                 layout.component_size));
            }
            const auto stride = view->byteStride
                                    ? static_cast<std::size_t>(*view->byteStride)
                                    : layout.element_size;
            if (stride % layout.component_size != 0
                || stride < layout.element_size) {
                fail(std::format("cannot use byteStride {} for {}-byte "
                                 "elements",
                                 stride,
                                 layout.element_size));
                continue;
            }
            const auto needed = offset + stride * (count - 1) + layout.element_size;
            if (needed > static_cast<std::size_t>(view->byteLength)) {
                fail(std::format("needs {} bytes, but bufferView {} has {}",
                                 needed,
                                 *accessor.bufferView,
                                 view->byteLength));
            }
        }

        if (!accessor.sparse) { continue; }
        const auto &sparse = *accessor.sparse;
        if (sparse.count < 1 || sparse.count > accessor.count) {
            fail(std::format("has sparse count {} (count is {})",
                             sparse.count,
                             accessor.count));
            continue;
        }
        const auto sparse_count = static_cast<std::size_t>(sparse.count);
        const auto index_type = sparse.indices.componentType;
        if (index_type != ComponentType::UNSIGNED_BYTE
            && index_type != ComponentType::UNSIGNED_SHORT
            && index_type != ComponentType::UNSIGNED_INT) {
            fail("has sparse indices that are not unsigned integers");
            continue;
        }
        const auto check_range = [&](std::string_view what,
                                     std::int64_t view_index,
                                     std::int64_t offset,
                                     std::size_t bytes) {
            const auto *view = find_view(manifest, view_index);
            if (view == nullptr) {
                fail(std::format("refers to missing bufferView {} for its "
                                 "sparse {}",
                                 view_index,
                                 what));
            } else if (static_cast<std::size_t>(offset) + bytes
                       > static_cast<std::size_t>(view->byteLength)) {
                fail(std::format("needs {} bytes of sparse {}, but "
                                 "bufferView {} has {}",
                                 static_cast<std::size_t>(offset) + bytes,
                                 what,
                                 view_index,
                                 view->byteLength));
            }
        };
        check_range("indices",
                    sparse.indices.bufferView,
                    sparse.indices.byteOffset,
                    sparse_count * component_size(index_type));
        check_range("values",
                    sparse.values.bufferView,
                    sparse.values.byteOffset,
                    sparse_count * layout.element_size);
    }
}

auto materialize_accessors(
    const glTF &manifest,
    const std::vector<std::span<const std::byte>> &buffer_views,
    LoadErrors &errors) -> std::vector<std::vector<std::byte>> {
    std::vector<std::vector<std::byte>> storage(manifest.accessors.size());
    for (const auto &[i, accessor] : std::views::enumerate(manifest.accessors)) {
        if (accessor.bufferView && !accessor.sparse) { continue; }
        const auto layout = accessor_layout(accessor);
        const auto count = static_cast<std::size_t>(accessor.count);
        auto &bytes = storage[static_cast<std::size_t>(i)];
        // Base values: the bufferView's elements, packed; or zeros.
        bytes.assign(count * layout.element_size, std::byte{0});
        if (accessor.bufferView) {
            const auto view_index = static_cast<std::size_t>(*accessor.bufferView);
            const auto &view = manifest.bufferViews[view_index];
            const auto source = buffer_views[view_index].subspan(
                static_cast<std::size_t>(accessor.byteOffset));
            const auto stride = view.byteStride
                                    ? static_cast<std::size_t>(*view.byteStride)
                                    : layout.element_size;
            for (std::size_t e = 0; e < count; ++e) {
                std::ranges::copy(source.subspan(e * stride, layout.element_size),
                                  bytes.begin()
                                      + static_cast<std::ptrdiff_t>(
                                          e * layout.element_size));
            }
        }

        if (!accessor.sparse) { continue; }
        const auto &sparse = *accessor.sparse;
        const auto sparse_count = static_cast<std::size_t>(sparse.count);
        const auto indices =
            buffer_views[static_cast<std::size_t>(sparse.indices.bufferView)]
                .subspan(static_cast<std::size_t>(sparse.indices.byteOffset));
        const auto values =
            buffer_views[static_cast<std::size_t>(sparse.values.bufferView)]
                .subspan(static_cast<std::size_t>(sparse.values.byteOffset));
        // Copies value j over element indices[j], checking the order.
        const auto substitute = [&](const auto &index_values) {
            std::optional<std::size_t> previous;
            for (std::size_t j = 0; j < sparse_count; ++j) {
                const std::size_t index = index_values[j];
                if (index >= count || (previous && index <= *previous)) {
                    errors.add(LoadError::Code::InvalidGltf,
                               std::format("accessor {} has sparse index {} at "
                                           "position {}: indices must be "
                                           "strictly increasing and less "
                                           "than {}",
                                           i,
                                           index,
                                           j,
                                           count));
                    return;
                }
                previous = index;
                std::ranges::copy(values.subspan(j * layout.element_size,
                                                 layout.element_size),
                                  bytes.begin()
                                      + static_cast<std::ptrdiff_t>(
                                          index * layout.element_size));
            }
        };
        // check_accessors allowed only these three index types.
        switch (sparse.indices.componentType) {
        case ComponentType::UNSIGNED_BYTE:
            substitute(packed_values<std::uint8_t>(indices, sparse_count));
            break;
        case ComponentType::UNSIGNED_SHORT:
            substitute(packed_values<std::uint16_t>(indices, sparse_count));
            break;
        default:
            substitute(packed_values<std::uint32_t>(indices, sparse_count));
            break;
        }
    }
    return storage;
}

} // namespace balsa::visualization::scene_graph::gltf::detail
