#pragma once
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <variant>

#include "types.hpp"

#include <zipper/DataArray.hpp>
#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::scene_graph {

// ── AttributeDomain ─────────────────────────────────────────────────
//
// Which mesh element an attribute is defined on.  The values match
// simplex dimension so UI code can display them uniformly.

enum class AttributeDomain : int8_t {
    Vertex = 0,
    Edge = 1,
    Face = 2,
};

// ── MeshAttribute ───────────────────────────────────────────────────
//
// A named attribute: count() elements of component_count() values each,
// stored in its own element type (see AttributeType).  Storage is a
// column-major zipper::DataArray with extents (component_count, count),
// i.e. one column per element, so memory is element-major
// (x0 y0 z0 x1 y1 z1 ...).  The array is the only source of the shape.
//
// Constructed from any rank-1 (scalar per element) or rank-2
// (components x elements) mdspan of arithmetic values, in any layout or
// accessor.  Values keep their type, widened to the nearest stored type
// (e.g. int16 -> int32, uint8 -> uint32).

enum class AttributeType : uint8_t {
    Float32,
    Float64,
    Int32,
    UInt32,
};

/// Why a typed view of an attribute could not be made, with what was
/// actually there.
struct ViewError {
    enum class Reason : uint8_t {
        WrongType,           ///< values are stored as actual_type
        WrongComponentCount, ///< values have actual_components components
    };
    Reason reason;
    AttributeType actual_type = AttributeType::Float32;
    uint8_t actual_components = 0;
};

namespace detail {
    /// Stored type for attribute values given as T; no `type` member when T
    /// is unsupported.
    template <typename T>
    struct attribute_storage {};
    template <>
    struct attribute_storage<float> {
        using type = float;
    };
    template <>
    struct attribute_storage<double> {
        using type = double;
    };
    template <std::signed_integral T>
        requires(sizeof(T) <= sizeof(int32_t))
    struct attribute_storage<T> {
        using type = int32_t;
    };
    template <std::unsigned_integral T>
        requires(sizeof(T) <= sizeof(uint32_t) && !std::same_as<T, bool>)
    struct attribute_storage<T> {
        using type = uint32_t;
    };

    template <typename T>
    using attribute_storage_t = typename attribute_storage<T>::type;
} // namespace detail

/// Value types MeshAttribute accepts: float, double, or an integer of at
/// most 32 bits (stored as float, double, int32 or uint32).
template <typename T>
concept AttributeValue =
    requires { typename detail::attribute_storage<std::remove_cv_t<T>>::type; };

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

    /// Largest component count representable by component_count().
    static constexpr std::size_t max_components =
        std::numeric_limits<uint8_t>::max();

    MeshAttribute() = default;
    /// Precondition: data.extent(0) <= max_components for rank-2 data.
    template <AttributeValue T,
              typename Extents,
              typename Layout,
              typename Accessor>
        requires(Extents::rank() == 1 || Extents::rank() == 2)
    MeshAttribute(std::string name,
                  AttributeDomain domain,
                  ::zipper::mdspan<T, Extents, Layout, Accessor> data);

    std::string name;
    AttributeDomain domain = AttributeDomain::Vertex;

    [[nodiscard]] auto type() const -> AttributeType {
        return static_cast<AttributeType>(m_values.index());
    }
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

    /// Extents (component_count(), count()).  Fails with WrongType unless
    /// the values are stored as T, and, for static D, with
    /// WrongComponentCount unless component_count() == D.
    template <typename T, std::size_t D = std::dynamic_extent>
    [[nodiscard]] auto view() const
        -> std::expected<const_view_type<T, D>, ViewError> {
        const auto *m = std::get_if<storage_type<T>>(&m_values);
        if (m == nullptr) {
            return std::unexpected(ViewError{ ViewError::Reason::WrongType,
                                              type(),
                                              component_count() });
        }
        const auto all = m->expression().as_mdspan();
        if constexpr (D == std::dynamic_extent) {
            return all;
        } else {
            if (all.extent(0) != D) {
                return std::unexpected(
                    ViewError{ ViewError::Reason::WrongComponentCount,
                               type(),
                               component_count() });
            }
            return const_view_type<T, D>(all.data_handle(), all.extent(1));
        }
    }

  private:
    // Alternative order matches AttributeType.
    std::variant<storage_type<float>,
                 storage_type<double>,
                 storage_type<int32_t>,
                 storage_type<uint32_t>>
        m_values;
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
