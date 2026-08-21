#pragma once

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
