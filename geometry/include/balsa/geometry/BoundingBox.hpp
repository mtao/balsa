#if !defined(BALSA_GEOMETRY_BOUNDINGBOX)
#define BALSA_GEOMETRY_BOUNDINGBOX

#if BALSA_HAS_QUIVER

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
    using aabb_type = quiver::spatial::AABB<static_cast<int8_t>(Dim)>;

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

#else// !BALSA_HAS_QUIVER — legacy zipper-only implementation

#include <zipper/types.hpp>
#include <zipper/Vector.hpp>
#include <bitset>
#include <zipper/as.hpp>
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/concepts/Vector.hpp>
#include <limits>


namespace balsa::geometry {
template<typename T, ::zipper::rank_type Dim = std::dynamic_extent>
class BoundingBox {
  private:
    using limits = std::numeric_limits<T>;

  public:
    template<
      ::zipper::concepts::Vector MinVec,
      ::zipper::concepts::Vector MaxVec>
    BoundingBox(const MinVec &m, const MaxVec &M);

    template<
      ::zipper::concepts::Vector Vec>
    BoundingBox(const Vec &m);
    BoundingBox() = default;

    BoundingBox(BoundingBox &&) = default;
    BoundingBox(const BoundingBox &) = default;
    BoundingBox &operator=(BoundingBox &&) = default;
    BoundingBox &operator=(const BoundingBox &) = default;

    BoundingBox(zipper::index_type dim) :

                                          m_min(::zipper::expression::nullary::Constant<T>(::zipper::create_dextents(dim), limits::max())),
                                          m_max(::zipper::expression::nullary::Constant<T>(::zipper::create_dextents(dim), limits::lowest())) {}

    template<::zipper::concepts::Vector Vec>
    void expand(const Vec &a);
    void expand(const BoundingBox &a);

    template<::zipper::concepts::Vector Vec>
    bool contains(const Vec &a) const;
    bool contains(const BoundingBox &a) const;

    auto range() const {
        return m_max - m_min;
    }

    zipper::Vector<T, Dim> corner(const std::bitset<Dim> &c) const;

    const auto &min() const { return m_min; }
    const auto &max() const { return m_max; }

  private:
    ::zipper::Vector<T, Dim> m_min = ::zipper::expression::nullary::Constant<T>(limits::max());
    ::zipper::Vector<T, Dim> m_max = ::zipper::expression::nullary::Constant<T>(limits::lowest());
};


// make sure the types are teh same and the extents are valid
template<
  ::zipper::concepts::Vector MinVec,
  ::zipper::concepts::Vector MaxVec>
    requires(std::is_same_v<typename MinVec::value_type, typename MaxVec::value_type>
             && (MinVec::extents_type::static_extent(0) == std::dynamic_extent
                 || MinVec::extents_type::static_extent(0) == std::dynamic_extent || MaxVec::extents_type::static_extent(0) == MinVec::extents_type::static_extent(0)))
BoundingBox(const MinVec &m, const MaxVec &M) -> BoundingBox<typename MinVec::value_type,
                                                             MinVec::extents_type::static_extent(0) == std::dynamic_extent ? MaxVec::extents_type::static_extent(0) : MinVec::extents_type::static_extent(0)>;


template<
  ::zipper::concepts::Vector Vec>
BoundingBox(const Vec &) -> BoundingBox<typename Vec::value_type,
                                        Vec::extents_type::static_extent(0)>;


template<typename T, zipper::rank_type Dim>
template<
  ::zipper::concepts::Vector MinVec,
  ::zipper::concepts::Vector MaxVec>
BoundingBox<T, Dim>::BoundingBox(const MinVec &m, const MaxVec &M) : m_min(m), m_max(M) {

    constexpr bool min_ext = MinVec::extents_type::static_extent(0) == std::dynamic_extent;
    constexpr bool max_ext = MaxVec::extents_type::static_extent(0) == std::dynamic_extent;
    if constexpr (min_ext && max_ext) {
        assert(m.extent(0) == M.extent(0));
    } else if constexpr (min_ext) {
        assert(m.extent(0) == Dim);
    } else if constexpr (max_ext) {
        assert(M.extent(0) == Dim);
    }
}

template<typename T, zipper::rank_type Dim>
template<
  ::zipper::concepts::Vector Vec>
BoundingBox<T, Dim>::BoundingBox(const Vec &m) : m_min(m), m_max(m) {
}

template<typename T, zipper::rank_type Dim>

auto BoundingBox<T, Dim>::corner(const std::bitset<Dim> &c) const -> zipper::Vector<T, Dim> {
    zipper::Vector<T, Dim> D;
    for (zipper::rank_type j = 0; j < Dim; ++j) {
        D(j) = c[j] ? max()(j) : min()(j);
    }
    return D;
}

template<typename T, zipper::rank_type Dim>
template<::zipper::concepts::Vector Vec>
void BoundingBox<T, Dim>::expand(const Vec &a) {

    m_min = ::zipper::as_vector(::zipper::min(a.as_array(), m_min.as_array()));
    m_max = ::zipper::as_vector(::zipper::max(a.as_array(), m_max.as_array()));
}

template<typename T, zipper::rank_type Dim>
void BoundingBox<T, Dim>::expand(const BoundingBox &a) {

    m_min = ::zipper::as_vector(::zipper::min(a.m_min.as_array(), m_min.as_array()));
    m_max = ::zipper::as_vector(::zipper::max(a.m_max.as_array(), m_max.as_array()));
}
template<typename T, zipper::rank_type Dim>
template<::zipper::concepts::Vector Vec>
bool BoundingBox<T, Dim>::contains(const Vec &x) const {

    auto xa = x.array();
    return ((m_min.array() <= xa) && (m_max.array() >= xa)).all();
}

template<typename T, zipper::rank_type Dim>
bool BoundingBox<T, Dim>::contains(const BoundingBox &o) const {

    return ((m_min.array() <= o.m_min.array()) && (m_max.array() >= o.m_max.array())).all();
}
}// namespace balsa::geometry

#endif// BALSA_HAS_QUIVER

#endif
