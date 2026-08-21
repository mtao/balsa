#if !defined(BALSA_GEOMETRY_BOUNDINGBOX)
#define BALSA_GEOMETRY_BOUNDINGBOX

// ============================================================================
// BoundingBox — thin wrapper around quiver::spatial::AABB
// ============================================================================
//
// BoundingBox<Dim> is always double-precision, matching quiver's KDOP
// internals.  It exposes a zipper-vector-based API (min(), max(),
// range(), corner(), expand(), contains()) on top of the underlying
// AABB<Dim>.
//
// The old BoundingBox<T, Dim> was templated on the scalar type; this
// version drops the T parameter (always double) but adds a conversion
// constructor from the AABB so that code can freely pass between the
// two representations.

#include <array>
#include <bitset>
#include <cstdint>
#include <span>

#include <zipper/Vector.hpp>
#include <zipper/concepts/Vector.hpp>

#include <quiver/spatial/KDOP.hpp>

namespace balsa::geometry {

template<::zipper::rank_type Dim>
class BoundingBox {
  public:
    using aabb_type = quiver::spatial::AABB<double, static_cast<int8_t>(Dim)>;

    // ── Construction ────────────────────────────────────────────────

    /// Default: empty box (ready for expand()).
    BoundingBox() : m_aabb(aabb_type::empty()) {}

    /// From two zipper vectors (min corner, max corner).
    template<::zipper::concepts::Vector MinVec,
             ::zipper::concepts::Vector MaxVec>
    BoundingBox(const MinVec &lo, const MaxVec &hi) : m_aabb(aabb_type::empty()) {
        std::array<double, Dim> lo_arr, hi_arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            lo_arr[j] = static_cast<double>(lo(j));
            hi_arr[j] = static_cast<double>(hi(j));
        }
        m_aabb.expand(std::span<const double, Dim>(lo_arr));
        m_aabb.expand(std::span<const double, Dim>(hi_arr));
    }

    /// From a single point (degenerate box).
    template<::zipper::concepts::Vector Vec>
    explicit BoundingBox(const Vec &pt) : m_aabb(aabb_type::empty()) {
        std::array<double, Dim> arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            arr[j] = static_cast<double>(pt(j));
        }
        m_aabb.expand(std::span<const double, Dim>(arr));
    }

    /// From an existing AABB (implicit conversion).
    BoundingBox(const aabb_type &aabb) : m_aabb(aabb) {}

    BoundingBox(BoundingBox &&) = default;
    BoundingBox(const BoundingBox &) = default;
    BoundingBox &operator=(BoundingBox &&) = default;
    BoundingBox &operator=(const BoundingBox &) = default;

    // ── Access to underlying AABB ───────────────────────────────────

    const aabb_type &aabb() const { return m_aabb; }
    aabb_type &aabb() { return m_aabb; }

    // ── min / max (return zipper vectors) ───────────────────────────

    auto min() const -> ::zipper::Vector<double, Dim> {
        ::zipper::Vector<double, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = m_aabb.min(static_cast<int8_t>(j));
        }
        return v;
    }

    auto max() const -> ::zipper::Vector<double, Dim> {
        ::zipper::Vector<double, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = m_aabb.max(static_cast<int8_t>(j));
        }
        return v;
    }

    // ── range (max - min) ───────────────────────────────────────────

    auto range() const -> ::zipper::Vector<double, Dim> {
        ::zipper::Vector<double, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = m_aabb.width(static_cast<int8_t>(j));
        }
        return v;
    }

    // ── corner ──────────────────────────────────────────────────────

    auto corner(const std::bitset<Dim> &c) const -> ::zipper::Vector<double, Dim> {
        ::zipper::Vector<double, Dim> v;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            v(j) = c[j] ? m_aabb.max(static_cast<int8_t>(j))
                        : m_aabb.min(static_cast<int8_t>(j));
        }
        return v;
    }

    // ── expand ──────────────────────────────────────────────────────

    template<::zipper::concepts::Vector Vec>
    void expand(const Vec &pt) {
        std::array<double, Dim> arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            arr[j] = static_cast<double>(pt(j));
        }
        m_aabb.expand(std::span<const double, Dim>(arr));
    }

    void expand(const BoundingBox &other) {
        m_aabb.merge_in_place(other.m_aabb);
    }

    // ── contains ────────────────────────────────────────────────────

    template<::zipper::concepts::Vector Vec>
    bool contains(const Vec &pt) const {
        std::array<double, Dim> arr;
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            arr[j] = static_cast<double>(pt(j));
        }
        return m_aabb.contains(std::span<const double, Dim>(arr));
    }

    bool contains(const BoundingBox &other) const {
        // A contains B iff merging B into A doesn't change A.
        // Equivalently: A.min(j) <= B.min(j) && A.max(j) >= B.max(j)
        // for all j.
        for (::zipper::rank_type j = 0; j < Dim; ++j) {
            auto axis = static_cast<int8_t>(j);
            if (m_aabb.min(axis) > other.m_aabb.min(axis)) return false;
            if (m_aabb.max(axis) < other.m_aabb.max(axis)) return false;
        }
        return true;
    }

    // ── is_empty ────────────────────────────────────────────────────

    bool is_empty() const { return m_aabb.is_empty(); }

  private:
    aabb_type m_aabb;
};

// ── CTAD deduction guides ───────────────────────────────────────────

template<::zipper::concepts::Vector MinVec,
         ::zipper::concepts::Vector MaxVec>
    requires(std::is_same_v<typename MinVec::value_type, typename MaxVec::value_type>)
BoundingBox(const MinVec &, const MaxVec &)
  -> BoundingBox<MinVec::extents_type::static_extent(0) == std::dynamic_extent
                   ? MaxVec::extents_type::static_extent(0)
                   : MinVec::extents_type::static_extent(0)>;

template<::zipper::concepts::Vector Vec>
BoundingBox(const Vec &) -> BoundingBox<Vec::extents_type::static_extent(0)>;

}// namespace balsa::geometry

#endif
