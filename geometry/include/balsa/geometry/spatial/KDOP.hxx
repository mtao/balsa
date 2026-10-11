#if !defined(BALSA_GEOMETRY_SPATIAL_KDOP_HXX)
#define BALSA_GEOMETRY_SPATIAL_KDOP_HXX
#include "KDOP.hpp"

#include <zipper/VectorBase.hxx>

namespace balsa::geometry::spatial {

// ============================================================================
// DefaultDirections specializations
// ============================================================================

// -- 2D, K=2: axis-aligned (AABB) -------------------------------------------
template <>
consteval auto DefaultDirections<2, 2>::directions()
    -> std::array<std::array<double, 2>, 2> {
    return {{
        {1.0, 0.0},
        {0.0, 1.0},
    }};
}

// -- 2D, K=4: axes + diagonals ----------------------------------------------
template <>
consteval auto DefaultDirections<2, 4>::directions()
    -> std::array<std::array<double, 2>, 4> {
    // Diagonal directions are not normalized — but that's fine for slab
    // computation; it only affects the scale of the projection intervals,
    // not the correctness of overlap tests or merge operations.  We keep
    // them unnormalized for cheaper projection (integer-like coefficients).
    return {{
        {1.0, 0.0},
        {0.0, 1.0},
        {1.0, 1.0},
        {1.0, -1.0},
    }};
}

// -- 3D, K=3: axis-aligned (AABB) -------------------------------------------
template <>
consteval auto DefaultDirections<3, 3>::directions()
    -> std::array<std::array<double, 3>, 3> {
    return {{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
    }};
}

// -- 3D, K=9: axes + face diagonals -----------------------------------------
template <>
consteval auto DefaultDirections<3, 9>::directions()
    -> std::array<std::array<double, 3>, 9> {
    return {{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
        {1.0, 1.0, 0.0},
        {1.0, 0.0, 1.0},
        {0.0, 1.0, 1.0},
        {1.0, -1.0, 0.0},
        {1.0, 0.0, -1.0},
        {0.0, 1.0, -1.0},
    }};
}

// -- 3D, K=13: axes + face diags + space diags ------------------------------
template <>
consteval auto DefaultDirections<3, 13>::directions()
    -> std::array<std::array<double, 3>, 13> {
    return {{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
        {1.0, 1.0, 0.0},
        {1.0, 0.0, 1.0},
        {0.0, 1.0, 1.0},
        {1.0, -1.0, 0.0},
        {1.0, 0.0, -1.0},
        {0.0, 1.0, -1.0},
        {1.0, 1.0, 1.0},
        {1.0, 1.0, -1.0},
        {1.0, -1.0, 1.0},
        {-1.0, 1.0, 1.0},
    }};
}

// ============================================================================
// KDOP implementation
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
consteval auto KDOP<T, SpatialDim, K, DirectionPolicy>::directions()
    -> std::array<std::array<double, SpatialDim>, K> {
    return DirectionPolicy::directions();
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
KDOP<T, SpatialDim, K, DirectionPolicy>::KDOP(slab_vector_type lo,
                                              slab_vector_type hi)
  : m_min(std::move(lo)), m_max(std::move(hi)) {}

// -- project() ---------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::project(
    std::span<const T, SpatialDim> point) -> slab_vector_type {
    // AABB fast path: for axis-aligned bounding boxes (K == SpatialDim),
    // projection is just copying coordinates — no direction multiplication
    // needed.  This avoids a float round-trip for integral T.
    if constexpr (K == SpatialDim) {
        slab_vector_type result;
        for (int8_t i = 0; i < SpatialDim; ++i) { result(i) = point[i]; }
        return result;
    } else {
        // General k-DOP path (floating-point only, enforced by static_assert).
        constexpr auto dirs = directions();

        // Check at compile time whether all direction coefficients are in
        // {-1, 0, +1}.  When true, projections reduce to additions and
        // subtractions — no multiplications required.
        constexpr bool unit_coeffs = [] {
            auto d = DirectionPolicy::directions();
            for (auto &dir : d)
                for (auto c : dir)
                    if (c != -1.0 && c != 0.0 && c != 1.0) return false;
            return true;
        }();

        slab_vector_type result;

        if constexpr (unit_coeffs) {
            for (int8_t i = 0; i < K; ++i) {
                T proj = T(0);
                for (int8_t j = 0; j < SpatialDim; ++j) {
                    double c = dirs[i][j];
                    if (c > 0.0) {
                        proj += point[j];
                    } else if (c < 0.0) {
                        proj -= point[j];
                    }
                }
                result(i) = proj;
            }
        } else {
            // General path: arbitrary direction coefficients require a dot
            // product.
            auto pt = ::zipper::VectorBase(point);
            for (int8_t i = 0; i < K; ++i) {
                auto dir = ::zipper::VectorBase(dirs[i]);
                result(i) = static_cast<T>(pt.dot(dir));
            }
        }

        return result;
    }
}

// -- empty() -----------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::empty() -> KDOP {
    slab_vector_type lo, hi;
    if constexpr (std::floating_point<T>) {
        constexpr T inf = std::numeric_limits<T>::infinity();
        for (int8_t i = 0; i < K; ++i) {
            lo(i) = +inf;
            hi(i) = -inf;
        }
    } else {
        // Integral types: use max/lowest as sentinels.
        for (int8_t i = 0; i < K; ++i) {
            lo(i) = std::numeric_limits<T>::max();
            hi(i) = std::numeric_limits<T>::lowest();
        }
    }
    return KDOP(std::move(lo), std::move(hi));
}

// -- from_point() ------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::from_point(
    std::span<const T, SpatialDim> point) -> KDOP {
    auto proj = project(point);
    return KDOP(proj, proj);
}

// -- fit() -------------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
template <typename PointRange>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::fit(const PointRange &points)
    -> KDOP {
    auto result = empty();
    for (const auto &pt : points) {
        result.expand(std::span<const T, SpatialDim>(pt));
    }
    return result;
}

// -- expand() ----------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void KDOP<T, SpatialDim, K, DirectionPolicy>::expand(
    std::span<const T, SpatialDim> point) {
    auto proj = project(point);
    m_min = ::zipper::min(m_min, proj);
    m_max = ::zipper::max(m_max, proj);
}

// -- inflate() ---------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void KDOP<T, SpatialDim, K, DirectionPolicy>::inflate(T margin) {
    m_min = m_min - margin;
    m_max = m_max + margin;
}

// -- merge_in_place() --------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void KDOP<T, SpatialDim, K, DirectionPolicy>::merge_in_place(
    const KDOP &other) {
    m_min = ::zipper::min(m_min, other.m_min);
    m_max = ::zipper::max(m_max, other.m_max);
}

// -- merge() -----------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::merge(const KDOP &other) const
    -> KDOP {
    auto result = *this;
    result.merge_in_place(other);
    return result;
}

// -- is_empty() --------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::is_empty() const -> bool {
    // An empty k-DOP has min > max on at least one slab (in practice, all
    // slabs are inverted after empty()).
    return (m_min > m_max).any();
}

// -- contains() --------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::contains(
    std::span<const T, SpatialDim> point) const -> bool {
    auto proj = project(point);
    return (proj >= m_min).all() && (proj <= m_max).all();
}

// -- overlaps() --------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::overlaps(const KDOP &other) const
    -> bool {
    return !((m_min > other.m_max) | (m_max < other.m_min)).any();
}

// -- volume() ----------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::volume() const -> T {
    // Use the first SpatialDim slab widths (axis-aligned) for the
    // volume approximation.  For AABBs this is exact.
    T vol = T(1);
    for (int8_t i = 0; i < SpatialDim; ++i) {
        T w = m_max(i) - m_min(i);
        if (w <= T(0)) return T(0);
        vol *= w;
    }
    return vol;
}

// -- surface_area() ----------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::surface_area() const -> T {
    if constexpr (SpatialDim == 2) {
        // Perimeter of the axis-aligned rectangle.
        T wx = m_max(0) - m_min(0);
        T wy = m_max(1) - m_min(1);
        return T(2) * (wx + wy);
    } else if constexpr (SpatialDim == 3) {
        // Surface area of the axis-aligned box.
        T wx = m_max(0) - m_min(0);
        T wy = m_max(1) - m_min(1);
        T wz = m_max(2) - m_min(2);
        return T(2) * (wx * wy + wx * wz + wy * wz);
    } else {
        // General fallback: sum of pairwise products of slab widths.
        T sa = T(0);
        for (int8_t i = 0; i < SpatialDim; ++i) {
            for (int8_t j = i + 1; j < SpatialDim; ++j) {
                sa += (m_max(i) - m_min(i)) * (m_max(j) - m_min(j));
            }
        }
        return T(2) * sa;
    }
}

// -- center() ----------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::center() const
    -> std::array<T, SpatialDim>
    requires(field<T>)
{
    // Use the axis-aligned slab midpoints as the center.
    std::array<T, SpatialDim> c;
    for (int8_t i = 0; i < SpatialDim; ++i) {
        c[i] = T(0.5) * (m_min(i) + m_max(i));
    }
    return c;
}

// -- diagonal() --------------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::diagonal() const -> T
    requires(K == SpatialDim && field<T>)
{
    return (m_max - m_min).norm();
}

// -- min_distance_sq() -------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::min_distance_sq(
    std::span<const T, SpatialDim> point) const -> T {
    // Use axis-aligned slab distances (first SpatialDim slabs).
    // For AABBs this is exact.  For general k-DOPs it underestimates,
    // which is safe for BVH nearest-neighbor pruning.
    T dist_sq = T(0);
    for (int8_t i = 0; i < SpatialDim; ++i) {
        T v = point[i];
        if (v < m_min(i)) {
            T d = m_min(i) - v;
            dist_sq += d * d;
        } else if (v > m_max(i)) {
            T d = v - m_max(i);
            dist_sq += d * d;
        }
    }
    return dist_sq;
}

// -- ray_intersect() ---------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::ray_intersect(
    std::span<const T, SpatialDim> origin,
    std::span<const T, SpatialDim> direction) const
    -> std::optional<std::pair<T, T>>
    requires(field<T>)
{
    constexpr auto dirs = directions();

    T t_near = -std::numeric_limits<T>::infinity();
    T t_far = std::numeric_limits<T>::infinity();

    for (int8_t i = 0; i < K; ++i) {
        // Project origin and direction onto slab direction i.
        T proj_origin = T(0);
        T proj_dir = T(0);
        for (int8_t j = 0; j < SpatialDim; ++j) {
            proj_origin += origin[j] * static_cast<T>(dirs[i][j]);
            proj_dir += direction[j] * static_cast<T>(dirs[i][j]);
        }

        T slab_lo = m_min(i);
        T slab_hi = m_max(i);

        T eps = std::numeric_limits<T>::epsilon() * T(100);
        if (std::abs(proj_dir) < eps) {
            // Ray is parallel to this slab.  Check if origin is inside.
            if (proj_origin < slab_lo || proj_origin > slab_hi) {
                return std::nullopt; // Outside this slab entirely.
            }
            // Inside this slab — no constraint on t.
            continue;
        }

        // Compute t-values where ray enters and exits this slab.
        T t1 = (slab_lo - proj_origin) / proj_dir;
        T t2 = (slab_hi - proj_origin) / proj_dir;

        // Ensure t1 <= t2.
        if (t1 > t2) { std::swap(t1, t2); }

        t_near = std::max(t_near, t1);
        t_far = std::min(t_far, t2);

        if (t_near > t_far) {
            return std::nullopt; // No overlap between slab intervals.
        }
    }

    return std::pair{t_near, t_far};
}

// -- Slab accessors ----------------------------------------------------------

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::min(int8_t axis) const -> T {
    return m_min(axis);
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::max(int8_t axis) const -> T {
    return m_max(axis);
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::width(int8_t axis) const -> T {
    return m_max(axis) - m_min(axis);
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::slab_min() const
    -> const slab_vector_type & {
    return m_min;
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto KDOP<T, SpatialDim, K, DirectionPolicy>::slab_max() const
    -> const slab_vector_type & {
    return m_max;
}

} // namespace balsa::geometry::spatial
#endif
