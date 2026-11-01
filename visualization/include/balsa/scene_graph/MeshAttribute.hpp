#pragma once
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <stdfloat>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "types.hpp"

#include <zipper/DataArray.hpp>
#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::scene_graph {

// ── AttributeDomain ─────────────────────────────────────────────────
//
// Which mesh element an attribute is defined on.

enum class AttributeDomain : int8_t {
    Vertex = 0,
    Edge = 1,
    Face = 2,
};

// ── MeshAttribute ───────────────────────────────────────────────────
//
// A named attribute holding up to 2-tensors per-element.
// (scalars are 1 x 1, k-vectors k x 1).
// Values are stored major in (row, column, simplex_index).

// Scalar component type of an attribute; values match VkComponentTypeKHR.
enum class AttributeType : uint8_t {
    Float16 = 0,
    Float32 = 1,
    Float64 = 2,
    Int8 = 3,
    Int16 = 4,
    Int32 = 5,
    Int64 = 6,
    UInt8 = 7,
    UInt16 = 8,
    UInt32 = 9,
    UInt64 = 10,
};

/// Why a typed view of an attribute could not be made, with what was
/// actually there.
struct WrongType {
    AttributeType actual;
};
struct WrongComponentCount {
    uint8_t actual;
};
struct WrongShape {
    uint8_t actual_rows;
    uint8_t actual_columns;
};
using ViewError = std::variant<WrongType, WrongComponentCount, WrongShape>;

namespace detail {
    // The stored value types; the index of each is its AttributeType.
    using attribute_values = std::tuple<std::float16_t,
                                        float,
                                        double,
                                        int8_t,
                                        int16_t,
                                        int32_t,
                                        int64_t,
                                        uint8_t,
                                        uint16_t,
                                        uint32_t,
                                        uint64_t>;

    // Index of T in attribute_values, or the tuple size if absent.
    template <typename T>
    inline constexpr std::size_t attribute_value_index = std::apply(
        [](auto... values) {
            const std::array matches{std::is_same_v<T, decltype(values)>...};
            return static_cast<std::size_t>(std::ranges::find(matches, true)
                                            - matches.begin());
        },
        attribute_values{});

    // sizeof each attribute_values type.
    inline constexpr auto attribute_sizes = std::apply(
        [](auto... values) { return std::array{sizeof(values)...}; },
        attribute_values{});
} // namespace detail

/// Value types MeshAttribute stores, exactly as given (no widening).
template <typename T>
concept AttributeValue =
    detail::attribute_value_index<std::remove_cv_t<T>>
    < std::tuple_size_v<detail::attribute_values>;

template <AttributeValue T>
inline constexpr AttributeType attribute_type_of = static_cast<AttributeType>(
    detail::attribute_value_index<std::remove_cv_t<T>>);

/// Bytes per component.
constexpr auto attribute_type_size(AttributeType type) -> std::size_t {
    return detail::attribute_sizes[std::to_underlying(type)];
}

/// Integer types that can be normalized (Vulkan UNORM / SNORM).
constexpr auto is_normalizable(AttributeType type) -> bool {
    return type == AttributeType::Int8 || type == AttributeType::Int16
           || type == AttributeType::UInt8 || type == AttributeType::UInt16;
}

class MeshAttribute {
  public:
    /// Plain column-major (components x count) storage for one attribute;
    /// no arithmetic semantics.  Wrap a view in zipper::MatrixBase for math.
    /// (zipper's "left-major" means leftmost index slowest, i.e. row-major;
    /// LeftMajor = false is column-major.  The public DataArray alias only
    /// covers static extents.)
    template <typename T>
    using storage_type = ::zipper::detail::
        DataArray_<T, ::zipper::dextents<2>, /*LeftMajor=*/false>;
    template <typename T, std::size_t D = std::dynamic_extent>
    using const_view_type =
        ::zipper::mdspan<const T,
                         ::zipper::extents<D, std::dynamic_extent>,
                         ::zipper::storage::col_major>;
    template <typename T,
              std::size_t Rows = std::dynamic_extent,
              std::size_t Cols = std::dynamic_extent>
    using const_tensor_view_type =
        ::zipper::mdspan<const T,
                         ::zipper::extents<Rows, Cols, std::dynamic_extent>,
                         ::zipper::storage::col_major>;

    /// Largest component count representable by component_count().
    static constexpr std::size_t max_components =
        std::numeric_limits<uint8_t>::max();

    MeshAttribute() = default;
    /// Precondition: at most max_components components per element, and
    /// `normalized` only for 8 / 16-bit integers.
    template <AttributeValue T,
              typename Extents,
              typename Layout,
              typename Accessor>
        requires(Extents::rank() >= 1 && Extents::rank() <= 3)
    MeshAttribute(std::string name,
                  AttributeDomain domain,
                  ::zipper::mdspan<T, Extents, Layout, Accessor> data,
                  bool normalized = false);

    std::string name;
    AttributeDomain domain = AttributeDomain::Vertex;

    [[nodiscard]] auto type() const -> AttributeType {
        return static_cast<AttributeType>(m_values.index());
    }
    /// Integer values represent [0, 1] (unsigned) or [-1, 1] (signed).
    [[nodiscard]] auto normalized() const -> bool { return m_normalized; }
    template <typename T>
    [[nodiscard]] auto holds() const -> bool {
        return std::holds_alternative<storage_type<T>>(m_values);
    }

    /// Calls f with a dynamic const view of the values, whatever their type.
    template <typename F>
    auto visit(F &&f) const -> decltype(auto) {
        return std::visit(
            [&](const auto &m) -> decltype(auto) {
                return f(m.expression().as_mdspan());
            },
            m_values);
    }

    [[nodiscard]] auto component_count() const -> uint8_t {
        return static_cast<uint8_t>(
            visit([](const auto &v) { return v.extent(0); }));
    }
    [[nodiscard]] auto count() const -> std::size_t {
        return visit([](const auto &v) { return v.extent(1); });
    }
    /// Element shape.
    [[nodiscard]] auto rows() const -> uint8_t {
        return static_cast<uint8_t>(component_count() / m_columns);
    }
    [[nodiscard]] auto columns() const -> uint8_t { return m_columns; }
    /// columns() == 1.
    [[nodiscard]] auto is_vector() const -> bool { return m_columns == 1; }
    /// Bytes per column and per element.
    [[nodiscard]] auto column_stride() const -> std::size_t {
        return rows() * attribute_type_size(type());
    }
    [[nodiscard]] auto element_size() const -> std::size_t {
        return component_count() * attribute_type_size(type());
    }

    /// Extents (component_count(), count()).  Fails with WrongType unless
    /// the values are stored as T, and, for static D, with
    /// WrongComponentCount unless component_count() == D.
    template <typename T, std::size_t D = std::dynamic_extent>
    [[nodiscard]] auto view() const
        -> std::expected<const_view_type<T, D>, ViewError> {
        const auto *m = std::get_if<storage_type<T>>(&m_values);
        if (m == nullptr) {
            return std::unexpected(WrongType{type()});
        }
        const auto all = m->expression().as_mdspan();
        if constexpr (D == std::dynamic_extent) {
            return all;
        } else {
            if (all.extent(0) != D) {
                return std::unexpected(WrongComponentCount{component_count()});
            }
            return const_view_type<T, D>(all.data_handle(), all.extent(1));
        }
    }

    /// Extents (rows(), columns(), count()); fails on the wrong type or
    /// shape.
    template <typename T,
              std::size_t Rows = std::dynamic_extent,
              std::size_t Cols = std::dynamic_extent>
    [[nodiscard]] auto tensor_view() const
        -> std::expected<const_tensor_view_type<T, Rows, Cols>, ViewError> {
        const auto all = view<T>();
        if (!all) { return std::unexpected(all.error()); }
        if ((Rows != std::dynamic_extent && Rows != rows())
            || (Cols != std::dynamic_extent && Cols != columns())) {
            return std::unexpected(WrongShape{rows(), columns()});
        }
        return const_tensor_view_type<T, Rows, Cols>(
            all->data_handle(), rows(), columns(), count());
    }

  private:
    // One storage_type per attribute_values entry, in the same order.
    template <typename Tuple>
    struct storage_variant;
    template <typename... Ts>
    struct storage_variant<std::tuple<Ts...>> {
        using type = std::variant<storage_type<Ts>...>;
    };

    typename storage_variant<detail::attribute_values>::type m_values;
    uint8_t m_columns = 1;
    bool m_normalized = false;
};

// ── MeshAttributeHandle ─────────────────────────────────────────────
//
// Cheap, copyable reference to an attribute owned by a MeshGeometry.
// Compares by identity.  Keeps the attribute's data alive even if it
// is later removed from the MeshGeometry (it is then simply detached).

class MeshAttributeHandle {
  public:
    MeshAttributeHandle() = default;

    auto valid() const -> bool { return _attr != nullptr; }
    explicit operator bool() const { return valid(); }
    auto operator*() const -> const MeshAttribute & { return *_attr; }
    auto operator->() const -> const MeshAttribute * { return _attr.get(); }
    auto operator==(const MeshAttributeHandle &o) const -> bool {
        return _attr == o._attr;
    }

  private:
    friend class MeshGeometry;
    explicit MeshAttributeHandle(std::shared_ptr<const MeshAttribute> a)
      : _attr(std::move(a)) {}
    std::shared_ptr<const MeshAttribute> _attr;
};

} // namespace balsa::scene_graph

#include "MeshAttribute.hxx"
