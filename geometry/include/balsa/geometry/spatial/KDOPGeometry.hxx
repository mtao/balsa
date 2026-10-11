#if !defined(BALSA_GEOMETRY_SPATIAL_KDOPGEOMETRY_HXX)
#define BALSA_GEOMETRY_SPATIAL_KDOPGEOMETRY_HXX
#include "KDOPGeometry.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/utils/determinant.hpp>
#include <zipper/utils/inverse.hpp>

namespace balsa::geometry::spatial {

namespace detail {

    /// Squared Euclidean distance between two points.
    template <typename T, int8_t SD>
    auto distance_sq(const std::array<T, SD> &a, const std::array<T, SD> &b)
        -> T {
        return (::zipper::VectorBase(a) - ::zipper::VectorBase(b))
            .template norm_powered<2>();
    }

    /// Compute the rank of a set of SD-dimensional vectors using incremental
    /// Gram-Schmidt orthogonalization.  Returns a value in [0, SD].
    template <int8_t SD>
    auto compute_rank(const std::vector<std::array<double, SD>> &vecs,
                      double tol = 1e-10) -> int {
        std::vector<std::array<double, SD>> basis;

        for (const auto &v : vecs) {
            // Project out the existing basis components.
            auto r = v;
            auto r_view = ::zipper::VectorBase(r);
            for (const auto &b : basis) {
                double dot = r_view.dot(::zipper::VectorBase(b));
                r_view -= dot * ::zipper::VectorBase(b);
            }
            // Accept if the residual is non-negligible.
            double norm_sq = r_view.template norm_powered<2>();
            if (norm_sq > tol * tol) {
                r_view /= std::sqrt(norm_sq);
                basis.push_back(r);
                if (static_cast<int>(basis.size()) == SD) break;
            }
        }

        return static_cast<int>(basis.size());
    }

} // namespace detail

// ============================================================================
// kdop_geometry implementation
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
    requires(field<T>)
auto kdop_geometry(const KDOP<T, SpatialDim, K, DirectionPolicy> &kdop)
    -> KDOPGeometry<T, SpatialDim> {
    constexpr int num_planes = 2 * static_cast<int>(K);
    constexpr double tol = 1e-10;
    constexpr auto dirs = KDOP<T, SpatialDim, K, DirectionPolicy>::directions();

    KDOPGeometry<T, SpatialDim> result;

    if (kdop.is_empty()) return result;

    // ---- Phase 1: enumerate candidate vertices ----
    //
    // A vertex is the intersection of SpatialDim boundary planes.
    // Each of the 2K planes is (direction_index, min/max side):
    //   plane 2*i     →  d_i · x = slab_min(i)
    //   plane 2*i + 1 →  d_i · x = slab_max(i)
    //
    // For each C(2K, SpatialDim) combination, solve the SpatialDim × SpatialDim
    // system and keep feasible solutions.

    struct CandidateVertex {
        std::array<T, SpatialDim> position;
        std::bitset<static_cast<size_t>(2 * K)> active_planes;
    };
    std::vector<CandidateVertex> candidates;

    // Standard combination iterator: combo[] holds SpatialDim sorted indices
    // from {0, ..., 2K-1}.
    std::array<int, SpatialDim> combo;
    for (int i = 0; i < SpatialDim; ++i) combo[i] = i;

    for (;;) {
        // Build the linear system A x = b.
        // Directions are always double; slab values are T.
        // We solve in double for numerical stability, then convert to T.
        ::zipper::Matrix<double, SpatialDim, SpatialDim> A(SpatialDim,
                                                           SpatialDim);
        ::zipper::Vector<double, SpatialDim> b(SpatialDim);

        for (int r = 0; r < SpatialDim; ++r) {
            int plane = combo[r];
            int dir_idx = plane / 2;
            bool is_max = (plane % 2) == 1;

            for (int c = 0; c < SpatialDim; ++c) { A(r, c) = dirs[dir_idx][c]; }
            b(r) = static_cast<double>(
                is_max ? kdop.max(static_cast<int8_t>(dir_idx))
                       : kdop.min(static_cast<int8_t>(dir_idx)));
        }

        // Skip singular systems (dependent directions) via determinant check.
        // For SpatialDim <= 3 this uses a closed-form O(1) determinant;
        // the inverse also uses a closed-form formula, avoiding the heap
        // allocation that PLU requires for its permutation vector.
        double det = ::zipper::utils::determinant(A);
        if (std::abs(det) > std::numeric_limits<double>::epsilon()) {
            auto x = ::zipper::utils::inverse(A) * b;

            std::array<T, SpatialDim> point;
            for (int i = 0; i < SpatialDim; ++i)
                point[i] = static_cast<T>(x(i));

            // Feasibility: point must lie inside all 2K slab constraints.
            bool feasible = true;
            std::bitset<static_cast<size_t>(2 * K)> active;

            for (int8_t d = 0; d < K; ++d) {
                double proj = ::zipper::VectorBase(dirs[d]).dot(
                    ::zipper::VectorBase(point));

                if (proj < static_cast<double>(kdop.min(d)) - tol
                    || proj > static_cast<double>(kdop.max(d)) + tol) {
                    feasible = false;
                    break;
                }

                // Track which boundary planes this vertex sits on.
                if (std::abs(proj - static_cast<double>(kdop.min(d))) < tol) {
                    active.set(static_cast<size_t>(2 * d));
                }
                if (std::abs(proj - static_cast<double>(kdop.max(d))) < tol) {
                    active.set(static_cast<size_t>(2 * d + 1));
                }
            }

            if (feasible) { candidates.push_back({point, active}); }
        }

        // Advance to the next combination.
        int i = SpatialDim - 1;
        while (i >= 0 && combo[i] == num_planes - SpatialDim + i) --i;
        if (i < 0) break;
        ++combo[i];
        for (int j = i + 1; j < SpatialDim; ++j) {
            combo[j] = combo[j - 1] + 1;
        }
    }

    // ---- Phase 2: deduplicate vertices ----
    //
    // Merge candidates within tolerance, taking the union of their active-plane
    // bitsets.

    struct UniqueVertex {
        std::array<T, SpatialDim> position;
        std::bitset<static_cast<size_t>(2 * K)> active_planes;
    };
    std::vector<UniqueVertex> vertices;

    constexpr T dedup_tol_sq = static_cast<T>(tol * tol);

    for (const auto &cand : candidates) {
        bool merged = false;
        for (auto &uv : vertices) {
            if (detail::distance_sq<T, SpatialDim>(cand.position, uv.position)
                < dedup_tol_sq) {
                uv.active_planes |= cand.active_planes;
                merged = true;
                break;
            }
        }
        if (!merged) {
            vertices.push_back({cand.position, cand.active_planes});
        }
    }

    // ---- Phase 3: populate output vertices ----

    result.vertices.reserve(vertices.size());
    for (const auto &v : vertices) { result.vertices.push_back(v.position); }

    // ---- Phase 4: find edges ----
    //
    // Two vertices are connected by an edge iff the direction vectors of their
    // common active boundary planes have rank exactly SpatialDim - 1.  This
    // means the common constraints define a 1-dimensional face (an edge) of the
    // polytope.

    const size_t nv = vertices.size();
    for (size_t vi = 0; vi < nv; ++vi) {
        for (size_t vj = vi + 1; vj < nv; ++vj) {
            auto common =
                vertices[vi].active_planes & vertices[vj].active_planes;

            // Collect the distinct direction vectors of the common planes.
            // Min-side and max-side of the same direction share the same
            // normal, so we deduplicate by direction index.
            std::vector<std::array<double, SpatialDim>> common_dirs;
            std::bitset<static_cast<size_t>(K)> seen_dirs;

            for (int p = 0; p < num_planes; ++p) {
                if (common.test(static_cast<size_t>(p))) {
                    int dir_idx = p / 2;
                    if (!seen_dirs.test(static_cast<size_t>(dir_idx))) {
                        seen_dirs.set(static_cast<size_t>(dir_idx));
                        common_dirs.push_back(dirs[dir_idx]);
                    }
                }
            }

            if (detail::compute_rank<SpatialDim>(common_dirs, tol)
                == SpatialDim - 1) {
                result.edges.emplace_back(vi, vj);
            }
        }
    }

    return result;
}

} // namespace balsa::geometry::spatial
#endif
