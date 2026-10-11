#if !defined(BALSA_GEOMETRY_SIMPLEX_RAY_INTERSECTION_HPP)
#define BALSA_GEOMETRY_SIMPLEX_RAY_INTERSECTION_HPP

/// @file ray_intersection.hpp
/// @brief Ray-simplex intersection and ray-edge closest approach.
///
/// Generic dimension-agnostic ray-simplex intersection:
///   - `ray_simplex_intersection(origin, direction, vertices)` finds
///     the intersection of a ray with a codimension-1 simplex
///     (edge in 2D, triangle in 3D, etc.).
///   - `ray_edge_closest_approach(origin, direction, a, b, tolerance)`
///     finds the closest approach between a ray and an edge segment
///     in arbitrary dimension (overdetermined case, e.g. edge in 3D).
///
/// The codimension-1 intersection solves the D×D linear system:
///   `origin + t * direction = v0 + sum_i(u_i * (v_i - v0))`
/// For D=2 this is ray vs edge; for D=3 this is Möller-Trumbore
/// ray vs triangle.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting
///     `std::span<const T, N>` to zipper via `VectorBase(span)` CTAD.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Result types
// ============================================================================

/// Result of a ray-simplex intersection (codimension-1).
///
/// @tparam D  Spatial dimension.  The simplex has D vertices
///            (a (D-1)-simplex in D-space).
///
/// The ray parameter `t` gives the hit distance along the ray:
///   `hit_point = origin + t * direction`.
///
/// The barycentric coordinates have D values summing to 1:
///   `hit_point = sum_i(barycentric[i] * vertex[i])`.
template <std::size_t D>
struct RaySimplexHit {
    /// Ray parameter (distance along ray direction).
    double t;

    /// Barycentric coordinates of the hit point on the simplex.
    /// All D values sum to 1.
    std::array<double, D> barycentric;
};

/// Result of a ray-edge closest approach (overdetermined case).
struct RayEdgeApproach {
    /// Ray parameter at closest approach.
    double t_ray;

    /// Edge parameter at closest approach (0 = vertex a, 1 = vertex b).
    double t_edge;

    /// Squared distance at closest approach.
    double distance_sq;
};

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

namespace detail {

    /// Ray vs edge in 2D (D=2): solve the 2×2 linear system.
    ///
    ///   origin + t * dir = v0 + u * (v1 - v0)
    ///   [-dir | (v1-v0)] * [t, u]^T = origin - v0
    ///
    /// Uses Cramer's rule on the 2×2 system.
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) == 2)
    auto ray_simplex_2d(const V &origin,
                        const V &direction,
                        const V &v0,
                        const V &v1,
                        bool backface_cull) -> std::optional<RaySimplexHit<2>> {
        using T = typename V::value_type;

        auto e = (v1 - v0).eval();
        auto rhs = (origin - v0).eval();

        // System: [-dir | e] * [t, u]^T = rhs
        // det = (-dir.x * e.y) - (-dir.y * e.x) = dir.y * e.x - dir.x * e.y
        T det = direction(1) * e(0) - direction(0) * e(1);

        // Backface culling: if det < 0, the ray enters from the "back" side
        // (assuming the edge normal points to the left of v0->v1).
        if (backface_cull && det < T{0}) { return std::nullopt; }

        constexpr T eps = T{1e-12};
        if (std::abs(det) < eps) {
            return std::nullopt; // Parallel ray.
        }

        T inv_det = T{1} / det;

        // t = (rhs.x * e.y - rhs.y * e.x) / det
        T t = (rhs(0) * e(1) - rhs(1) * e(0)) * inv_det;

        // u = det(A_u) / det where A_u replaces column 2 of A with rhs:
        //   det(A_u) = (-dir.x)*rhs.y - rhs.x*(-dir.y)
        //            = dir.y * rhs.x - dir.x * rhs.y
        T u = (direction(1) * rhs(0) - direction(0) * rhs(1)) * inv_det;

        if (u < T{0} || u > T{1}) {
            return std::nullopt; // Outside edge.
        }

        RaySimplexHit<2> hit;
        hit.t = static_cast<double>(t);
        hit.barycentric = {static_cast<double>(T{1} - u),
                           static_cast<double>(u)};
        return hit;
    }

    /// Ray vs triangle in 3D (D=3): Möller-Trumbore algorithm.
    ///
    /// Solves the 3×3 system:
    ///   origin + t * dir = v0 + u * (v1 - v0) + v * (v2 - v0)
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) == 3)
    auto ray_simplex_3d(const V &origin,
                        const V &direction,
                        const V &v0,
                        const V &v1,
                        const V &v2,
                        bool backface_cull) -> std::optional<RaySimplexHit<3>> {
        using T = typename V::value_type;

        auto e1 = (v1 - v0).eval();
        auto e2 = (v2 - v0).eval();

        auto pvec = direction.cross(e2).eval();
        T det = e1.dot(pvec);

        if (backface_cull && det < T{0}) { return std::nullopt; }

        constexpr T eps = T{1e-12};
        if (std::abs(det) < eps) {
            return std::nullopt; // Parallel ray.
        }

        T inv_det = T{1} / det;

        auto tvec = (origin - v0).eval();
        T u = tvec.dot(pvec) * inv_det;
        if (u < T{0} || u > T{1}) { return std::nullopt; }

        auto qvec = tvec.cross(e1).eval();
        T v = direction.dot(qvec) * inv_det;
        if (v < T{0} || u + v > T{1}) { return std::nullopt; }

        T t = e2.dot(qvec) * inv_det;

        RaySimplexHit<3> hit;
        hit.t = static_cast<double>(t);
        hit.barycentric = {static_cast<double>(T{1} - u - v),
                           static_cast<double>(u),
                           static_cast<double>(v)};
        return hit;
    }

    /// General ray vs (D-1)-simplex in D-space via Cramer's rule.
    ///
    /// Solves the D×D system:
    ///   [-dir | e1 | e2 | ... | e_{D-1}] * [t, u1, ..., u_{D-1}]^T = rhs
    /// where e_i = v_i - v_0, rhs = origin - v_0.
    ///
    /// Uses cofactor expansion (generalized Cramer's rule) via
    /// the relationship between cross products, triple products,
    /// and determinants in arbitrary dimensions.
    ///
    /// For D >= 4, we build the matrix explicitly and solve.
    template <::zipper::concepts::Vector V, std::size_t D>
        requires(D >= 4)
    auto ray_simplex_general(
        const V &origin,
        const V &direction,
        const std::array<::zipper::Vector<typename V::value_type, D>, D> &verts,
        bool backface_cull) -> std::optional<RaySimplexHit<D>> {
        using T = typename V::value_type;

        // Build the D×D matrix: columns are [-dir, e1, e2, ..., e_{D-1}]
        // where e_i = verts[i] - verts[0].
        // Use Gaussian elimination with partial pivoting.
        std::array<std::array<T, D + 1>, D> augmented; // D rows × (D+1) cols

        auto rhs = (origin - verts[0]).eval();

        for (std::size_t row = 0; row < D; ++row) {
            augmented[row][0] = -direction(row);
            for (std::size_t col = 1; col < D; ++col) {
                augmented[row][col] = verts[col](row) - verts[0](row);
            }
            augmented[row][D] = rhs(row);
        }

        // Gaussian elimination with partial pivoting.
        for (std::size_t col = 0; col < D; ++col) {
            // Find pivot.
            std::size_t pivot = col;
            T max_val = std::abs(augmented[col][col]);
            for (std::size_t row = col + 1; row < D; ++row) {
                T val = std::abs(augmented[row][col]);
                if (val > max_val) {
                    max_val = val;
                    pivot = row;
                }
            }

            constexpr T eps = T{1e-12};
            if (max_val < eps) {
                return std::nullopt; // Singular — ray parallel to simplex.
            }

            if (pivot != col) { std::swap(augmented[col], augmented[pivot]); }

            // Eliminate below pivot.
            T pivot_val = augmented[col][col];
            for (std::size_t row = col + 1; row < D; ++row) {
                T factor = augmented[row][col] / pivot_val;
                for (std::size_t j = col; j <= D; ++j) {
                    augmented[row][j] -= factor * augmented[col][j];
                }
            }
        }

        // Back-substitution.
        std::array<T, D> solution;
        for (std::size_t i = D; i-- > 0;) {
            T sum = augmented[i][D];
            for (std::size_t j = i + 1; j < D; ++j) {
                sum -= augmented[i][j] * solution[j];
            }
            solution[i] = sum / augmented[i][i];
        }

        // solution[0] = t, solution[1..D-1] = u1..u_{D-1}
        T t = solution[0];
        (void)t;

        // Check barycentric coordinates.
        T bary_sum = T{0};
        for (std::size_t i = 1; i < D; ++i) {
            if (solution[i] < T{0}) { return std::nullopt; }
            bary_sum += solution[i];
        }
        if (bary_sum > T{1}) { return std::nullopt; }

        // Backface culling: for the general case, check the sign of the
        // determinant.  If det < 0, the ray enters from the back.
        // The determinant's sign is the product of the diagonal of the
        // upper triangular matrix.
        if (backface_cull) {
            T det_sign = T{1};
            for (std::size_t i = 0; i < D; ++i) { det_sign *= augmented[i][i]; }
            if (det_sign < T{0}) { return std::nullopt; }
        }

        RaySimplexHit<D> hit;
        hit.t = static_cast<double>(solution[0]);
        hit.barycentric[0] = static_cast<double>(T{1} - bary_sum);
        for (std::size_t i = 1; i < D; ++i) {
            hit.barycentric[i] = static_cast<double>(solution[i]);
        }
        return hit;
    }

} // namespace detail

/// Ray vs codimension-1 simplex intersection (generic).
///
/// Finds the intersection of a ray with a (D-1)-simplex defined by
/// D vertices in D-dimensional space.  Returns the ray parameter `t`
/// and barycentric coordinates if the ray hits the simplex interior,
/// or `std::nullopt` if it misses.
///
/// Specialized fast paths:
///   - D=2: ray vs edge in 2D (Cramer's rule on 2×2 system)
///   - D=3: ray vs triangle in 3D (Möller-Trumbore algorithm)
///   - D>=4: Gaussian elimination on the D×D system
///
/// @param origin     Ray origin.
/// @param direction  Ray direction (not necessarily normalized).
/// @param verts      D vertices of the (D-1)-simplex.
/// @param backface_cull  If true, reject hits where the ray enters
///                       from the "back" side (negative determinant).
/// @return `RaySimplexHit<D>` if the ray hits, `std::nullopt` otherwise.
///         Note: `t` may be negative (hit is behind the ray origin).
template <::zipper::concepts::Vector V>
auto ray_simplex_intersection(
    const V &origin,
    const V &direction,
    const std::array<::zipper::Vector<typename V::value_type,
                                      V::extents_type::static_extent(0)>,
                     V::extents_type::static_extent(0)> &verts,
    bool backface_cull = false)
    -> std::optional<RaySimplexHit<V::extents_type::static_extent(0)>> {
    constexpr auto D = V::extents_type::static_extent(0);

    if constexpr (D == 2) {
        return detail::ray_simplex_2d(
            origin, direction, verts[0], verts[1], backface_cull);
    } else if constexpr (D == 3) {
        return detail::ray_simplex_3d(
            origin, direction, verts[0], verts[1], verts[2], backface_cull);
    } else {
        return detail::ray_simplex_general<V, D>(
            origin, direction, verts, backface_cull);
    }
}

/// Ray-edge closest approach in arbitrary dimension.
///
/// For an edge [a, b] in D-space (D >= 2), finds the closest approach
/// between the ray and the edge segment.  This handles the overdetermined
/// case (e.g. a ray and an edge in 3D that are skew lines).
///
/// Solves the least-squares problem of minimizing
///   ||origin + t_ray * dir - (a + t_edge * (b - a))||^2
/// subject to t_edge clamped to [0, 1].
///
/// @param origin     Ray origin.
/// @param direction  Ray direction (not necessarily normalized).
/// @param a, b       Edge endpoints.
/// @param tolerance  Maximum distance for a "hit".
/// @return `RayEdgeApproach` if the closest approach distance is within
///         tolerance, `std::nullopt` otherwise.
template <::zipper::concepts::Vector V>
auto ray_edge_closest_approach(const V &origin,
                               const V &direction,
                               const V &a,
                               const V &b,
                               double tolerance)
    -> std::optional<RayEdgeApproach> {
    using T = typename V::value_type;

    auto d = (b - a).eval(); // Edge direction.
    auto w = (origin - a).eval();

    T dd = direction.dot(direction); // ||dir||^2
    T de = direction.dot(d); // dir . edge
    T ee = d.dot(d); // ||edge||^2
    T dw = direction.dot(w); // dir . w
    T ew = d.dot(w); // edge . w

    T denom = dd * ee - de * de;

    T t_ray, t_edge;
    constexpr T eps = T{1e-12};

    if (std::abs(denom) < eps) {
        // Ray and edge are parallel.  Find closest point on edge to
        // ray origin.
        t_ray = T{0};
        t_edge = (ee > eps) ? ew / ee : T{0};
    } else {
        // General case: solve the 2×2 system.
        t_ray = (de * ew - ee * dw) / denom;
        t_edge = (dd * ew - de * dw) / denom;
    }

    // Clamp t_edge to [0, 1] and recompute t_ray.
    t_edge = std::clamp(t_edge, T{0}, T{1});

    // Re-solve for t_ray given clamped t_edge:
    // t_ray = (dir . (a + t_edge * d - origin)) / (dir . dir)
    //       = (dir . (a - origin) + t_edge * (dir . d)) / (dir . dir)
    if (dd > eps) {
        t_ray = (-dw + t_edge * de) / dd;
    } else {
        t_ray = T{0};
    }

    // Compute the closest points and distance.
    auto p_ray = (origin + t_ray * direction).eval();
    auto p_edge = (a + t_edge * d).eval();
    auto diff = (p_ray - p_edge).eval();
    double dist_sq = static_cast<double>(diff.dot(diff));

    if (dist_sq > tolerance * tolerance) { return std::nullopt; }

    return RayEdgeApproach{
        static_cast<double>(t_ray), static_cast<double>(t_edge), dist_sq};
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Ray vs codimension-1 simplex intersection (PointSpan wrapper).
template <PointSpan S>
    requires(dim_v<S> >= 2)
auto ray_simplex_intersection(
    S origin,
    S direction,
    const std::array<std::array<scalar_t<S>, dim_v<S>>, dim_v<S>> &verts,
    bool backface_cull = false) -> std::optional<RaySimplexHit<dim_v<S>>> {
    using T = scalar_t<S>;
    constexpr auto D = dim_v<S>;
    using Vec = ::zipper::Vector<T, D>;

    auto o = Vec(::zipper::VectorBase(origin));
    auto d = Vec(::zipper::VectorBase(direction));
    std::array<Vec, D> zverts;
    for (std::size_t i = 0; i < D; ++i) {
        zverts[i] = Vec(::zipper::VectorBase(std::span<const T, D>(verts[i])));
    }
    return ray_simplex_intersection(o, d, zverts, backface_cull);
}

/// Ray-edge closest approach (PointSpan wrapper).
template <PointSpan S>
auto ray_edge_closest_approach(S origin,
                               S direction,
                               S a,
                               S b,
                               double tolerance)
    -> std::optional<RayEdgeApproach> {
    return ray_edge_closest_approach(::zipper::VectorBase(origin),
                                     ::zipper::VectorBase(direction),
                                     ::zipper::VectorBase(a),
                                     ::zipper::VectorBase(b),
                                     tolerance);
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Ray vs codimension-1 simplex intersection (HasAsTuple wrapper).
///
/// The first two tuple elements are the ray origin and direction.
/// The remaining D elements are the simplex vertices.
template <HasAsTuple ID>
auto ray_simplex_intersection(const ID &id, bool backface_cull = false) {
    return std::apply(
        [backface_cull](
            const auto &origin, const auto &direction, const auto &...vs) {
            using T = std::remove_const_t<
                typename std::decay_t<decltype(origin)>::value_type>;
            constexpr auto D = std::decay_t<decltype(origin)>::extent;
            using Vec = ::zipper::Vector<T, D>;

            static_assert(
                sizeof...(vs) == D,
                "HasAsTuple must contain origin + direction + D vertices");

            auto o = Vec(::zipper::VectorBase(origin));
            auto d = Vec(::zipper::VectorBase(direction));
            std::array<Vec, D> verts = {Vec(::zipper::VectorBase(vs))...};
            return ray_simplex_intersection(o, d, verts, backface_cull);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
