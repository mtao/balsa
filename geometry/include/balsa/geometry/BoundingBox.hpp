#pragma once

#if BALSA_HAS_QUIVER

// ============================================================================
// BoundingBox<T, Dim> — thin wrapper around quiver::spatial::AABB<T, Dim>
// ============================================================================
//
// Provides a zipper-vector-based API (min(), max(), range(), corner(),
// expand(), contains()) on top of quiver's AABB.  The scalar type T is
// explicit — no default — matching quiver's convention.
//
// Per-axis accessors: min(axis), max(axis), range(axis).
// Convenience names:  width(), height(), depth().

#include <array>
#include <bitset>
#include <cstdint>
#include <span>

#include <zipper/Vector.hpp>
#include <zipper/concepts/Vector.hpp>

#include <quiver/spatial/KDOP.hpp>

namespace balsa::geometry {

template <typename T, ::zipper::rank_type Dim>
class BoundingBox {
  public:
    using value_type = T;
    using aabb_type = quiver::spatial::AABB<T, static_cast<int8_t>(Dim)>;

    // ── Construction ────────────────────────────────────────────────

    /// Default: empty box (ready for expand()).
    BoundingBox() : m_aabb(aabb_type::empty()) {}

    /// From two zipper vectors (min corner, max corner).
    template <::zipper::concepts::Vector MinVec,
              ::zipper::concepts::Vector MaxVec>
    BoundingBox(const MinVec &lo, const MaxVec &hi)
      : m_aabb(aabb_type::empty()) {
        std::array<T, Dim> lo_arr, hi_arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            lo_arr[j] = static_cast<T>(lo(j));
            hi_arr[j] = static_cast<T>(hi(j));
        }
        m_aabb.expand(std::span<const T, Dim>(lo_arr));
        m_aabb.expand(std::span<const T, Dim>(hi_arr));
    }

    /// From a single point (degenerate box).
    template <::zipper::concepts::Vector Vec>
    explicit BoundingBox(const Vec &pt) : m_aabb(aabb_type::empty()) {
        std::array<T, Dim> arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            arr[j] = static_cast<T>(pt(j));
        }
        m_aabb.expand(std::span<const T, Dim>(arr));
    }

    /// From explicit min/max values (for integral types like uint32_t).
    /// Only available when Dim == 2.
    BoundingBox(T x_min, T y_min, T x_max, T y_max)
        requires(Dim == 2)
      : m_aabb(aabb_type::empty()) {
        std::array<T, 2> lo_arr{x_min, y_min};
        std::array<T, 2> hi_arr{x_max, y_max};
        m_aabb.expand(std::span<const T, 2>(lo_arr));
        m_aabb.expand(std::span<const T, 2>(hi_arr));
    }

    /// From an existing AABB (implicit conversion).
    BoundingBox(const aabb_type &aabb) : m_aabb(aabb) {}

    BoundingBox(BoundingBox &&) = default;
    BoundingBox(const BoundingBox &) = default;
    auto operator=(BoundingBox &&) -> BoundingBox & = default;
    auto operator=(const BoundingBox &) -> BoundingBox & = default;

    // ── Access to underlying AABB ───────────────────────────────────

    auto aabb() const -> const aabb_type & { return m_aabb; }
    auto aabb() -> aabb_type & { return m_aabb; }

    // ── min / max (full vector) ─────────────────────────────────────

    auto min() const -> ::zipper::Vector<T, Dim> {
        ::zipper::Vector<T, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = m_aabb.min(static_cast<int8_t>(j));
        }
        return v;
    }

    auto max() const -> ::zipper::Vector<T, Dim> {
        ::zipper::Vector<T, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = m_aabb.max(static_cast<int8_t>(j));
        }
        return v;
    }

    // ── Per-axis accessors ──────────────────────────────────────────

    auto min(::zipper::rank_type axis) const -> T {
        return m_aabb.min(static_cast<int8_t>(axis));
    }

    auto max(::zipper::rank_type axis) const -> T {
        return m_aabb.max(static_cast<int8_t>(axis));
    }

    auto range(::zipper::rank_type axis) const -> T {
        return m_aabb.width(static_cast<int8_t>(axis));
    }

    // ── range (full vector) ─────────────────────────────────────────

    auto range() const -> ::zipper::Vector<T, Dim> {
        ::zipper::Vector<T, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = m_aabb.width(static_cast<int8_t>(j));
        }
        return v;
    }

    // ── Convenience dimension names ─────────────────────────────────

    auto width() const -> T { return range(0); }

    auto height() const -> T
        requires(Dim >= 2)
    {
        return range(1);
    }

    auto depth() const -> T
        requires(Dim >= 3)
    {
        return range(2);
    }

    // ── corner ──────────────────────────────────────────────────────

    auto corner(const std::bitset<Dim> &c) const -> ::zipper::Vector<T, Dim> {
        ::zipper::Vector<T, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = c[j] ? m_aabb.max(static_cast<int8_t>(j))
                        : m_aabb.min(static_cast<int8_t>(j));
        }
        return v;
    }

    // ── expand ──────────────────────────────────────────────────────

    template <::zipper::concepts::Vector Vec>
    auto expand(const Vec &pt) -> void {
        std::array<T, Dim> arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            arr[j] = static_cast<T>(pt(j));
        }
        m_aabb.expand(std::span<const T, Dim>(arr));
    }

    auto expand(const BoundingBox &other) -> void {
        m_aabb.merge_in_place(other.m_aabb);
    }

    // ── contains ────────────────────────────────────────────────────

    template <::zipper::concepts::Vector Vec>
    auto contains(const Vec &pt) const -> bool {
        std::array<T, Dim> arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            arr[j] = static_cast<T>(pt(j));
        }
        return m_aabb.contains(std::span<const T, Dim>(arr));
    }

    auto contains(const BoundingBox &other) const -> bool {
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            auto axis = static_cast<int8_t>(j);
            if (m_aabb.min(axis) > other.m_aabb.min(axis)) return false;
            if (m_aabb.max(axis) < other.m_aabb.max(axis)) return false;
        }
        return true;
    }

    // ── is_empty ────────────────────────────────────────────────────

    auto is_empty() const -> bool { return m_aabb.is_empty(); }

  private:
    aabb_type m_aabb;
};

// ── CTAD deduction guides ───────────────────────────────────────────

template <::zipper::concepts::Vector MinVec, ::zipper::concepts::Vector MaxVec>
    requires(std::is_same_v<typename MinVec::value_type,
                            typename MaxVec::value_type>)
BoundingBox(const MinVec &, const MaxVec &)
    -> BoundingBox<typename MinVec::value_type,
                   MinVec::extents_type::static_extent(0) == std::dynamic_extent
                       ? MaxVec::extents_type::static_extent(0)
                       : MinVec::extents_type::static_extent(0)>;

template <::zipper::concepts::Vector Vec>
BoundingBox(const Vec &) -> BoundingBox<typename Vec::value_type,
                                        Vec::extents_type::static_extent(0)>;

} // namespace balsa::geometry

#else // !BALSA_HAS_QUIVER — legacy zipper-only implementation

#include <bitset>
#include <limits>
#include <zipper/Vector.hpp>
#include <zipper/as.hpp>
#include <zipper/concepts/Vector.hpp>
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/types.hpp>

namespace balsa::geometry {
template <typename T, ::zipper::rank_type Dim = std::dynamic_extent>
class BoundingBox {
  private:
    using limits = std::numeric_limits<T>;

  public:
    using value_type = T;

    template <::zipper::concepts::Vector MinVec,
              ::zipper::concepts::Vector MaxVec>
    BoundingBox(const MinVec &m, const MaxVec &M);

    template <::zipper::concepts::Vector Vec>
    BoundingBox(const Vec &m);
    BoundingBox() = default;

    BoundingBox(BoundingBox &&) = default;
    BoundingBox(const BoundingBox &) = default;
    auto operator=(BoundingBox &&) -> BoundingBox & = default;
    auto operator=(const BoundingBox &) -> BoundingBox & = default;

    BoundingBox(zipper::index_type dim)
      :

        m_min(::zipper::expression::nullary::Constant<T>(
            ::zipper::create_dextents(dim),
            limits::max())),
        m_max(::zipper::expression::nullary::Constant<T>(
            ::zipper::create_dextents(dim),
            limits::lowest())) {}

    template <::zipper::concepts::Vector Vec>
    auto expand(const Vec &a) -> void;
    auto expand(const BoundingBox &a) -> void;

    template <::zipper::concepts::Vector Vec>
    auto contains(const Vec &a) const -> bool;
    auto contains(const BoundingBox &a) const -> bool;

    auto range() const { return m_max - m_min; }

    // Per-axis accessors.
    auto min(::zipper::rank_type axis) const -> T { return m_min(axis); }
    auto max(::zipper::rank_type axis) const -> T { return m_max(axis); }
    auto range(::zipper::rank_type axis) const -> T {
        return m_max(axis) - m_min(axis);
    }

    // Convenience dimension names.
    auto width() const -> T { return range(0); }
    auto height() const -> T
        requires(Dim >= 2)
    {
        return range(1);
    }
    auto depth() const -> T
        requires(Dim >= 3)
    {
        return range(2);
    }

    auto corner(const std::bitset<Dim> &c) const -> zipper::Vector<T, Dim>;

    auto min() const -> const auto & { return m_min; }
    auto max() const -> const auto & { return m_max; }

  private:
    ::zipper::Vector<T, Dim> m_min =
        ::zipper::expression::nullary::Constant<T>(limits::max());
    ::zipper::Vector<T, Dim> m_max =
        ::zipper::expression::nullary::Constant<T>(limits::lowest());
};

// make sure the types are teh same and the extents are valid
template <::zipper::concepts::Vector MinVec, ::zipper::concepts::Vector MaxVec>
    requires(
        std::is_same_v<typename MinVec::value_type, typename MaxVec::value_type>
        && (MinVec::extents_type::static_extent(0) == std::dynamic_extent
            || MinVec::extents_type::static_extent(0) == std::dynamic_extent
            || MaxVec::extents_type::static_extent(0)
                   == MinVec::extents_type::static_extent(0)))
BoundingBox(const MinVec &m, const MaxVec &M)
    -> BoundingBox<typename MinVec::value_type,
                   MinVec::extents_type::static_extent(0) == std::dynamic_extent
                       ? MaxVec::extents_type::static_extent(0)
                       : MinVec::extents_type::static_extent(0)>;

template <::zipper::concepts::Vector Vec>
BoundingBox(const Vec &) -> BoundingBox<typename Vec::value_type,
                                        Vec::extents_type::static_extent(0)>;

template <typename T, zipper::rank_type Dim>
template <::zipper::concepts::Vector MinVec, ::zipper::concepts::Vector MaxVec>
BoundingBox<T, Dim>::BoundingBox(const MinVec &m, const MaxVec &M)
  : m_min(m), m_max(M) {
    constexpr bool min_ext =
        MinVec::extents_type::static_extent(0) == std::dynamic_extent;
    constexpr bool max_ext =
        MaxVec::extents_type::static_extent(0) == std::dynamic_extent;
    if constexpr (min_ext && max_ext) {
        assert(m.extent(0) == M.extent(0));
    } else if constexpr (min_ext) {
        assert(m.extent(0) == Dim);
    } else if constexpr (max_ext) {
        assert(M.extent(0) == Dim);
    }
}

template <typename T, zipper::rank_type Dim>
template <::zipper::concepts::Vector Vec>
BoundingBox<T, Dim>::BoundingBox(const Vec &m) : m_min(m), m_max(m) {}

template <typename T, zipper::rank_type Dim>

auto BoundingBox<T, Dim>::corner(const std::bitset<Dim> &c) const
    -> zipper::Vector<T, Dim> {
    zipper::Vector<T, Dim> D;
    for (zipper::rank_type j = 0; j < Dim; ++j) {
        D(j) = c[j] ? max()(j) : min()(j);
    }
    return D;
}

template <typename T, zipper::rank_type Dim>
template <::zipper::concepts::Vector Vec>
auto BoundingBox<T, Dim>::expand(const Vec &a) -> void {
    m_min = ::zipper::as_vector(::zipper::min(a.as_array(), m_min.as_array()));
    m_max = ::zipper::as_vector(::zipper::max(a.as_array(), m_max.as_array()));
}

template <typename T, zipper::rank_type Dim>
auto BoundingBox<T, Dim>::expand(const BoundingBox &a) -> void {
    m_min = ::zipper::as_vector(
        ::zipper::min(a.m_min.as_array(), m_min.as_array()));
    m_max = ::zipper::as_vector(
        ::zipper::max(a.m_max.as_array(), m_max.as_array()));
}
template <typename T, zipper::rank_type Dim>
template <::zipper::concepts::Vector Vec>
auto BoundingBox<T, Dim>::contains(const Vec &x) const -> bool {
    auto xa = x.array();
    return ((m_min.array() <= xa) && (m_max.array() >= xa)).all();
}

template <typename T, zipper::rank_type Dim>
auto BoundingBox<T, Dim>::contains(const BoundingBox &o) const -> bool {
    return ((m_min.array() <= o.m_min.array())
            && (m_max.array() >= o.m_max.array()))
        .all();
}
} // namespace balsa::geometry

#endif // BALSA_HAS_QUIVER
