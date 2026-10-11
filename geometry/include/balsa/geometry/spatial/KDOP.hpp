#if !defined(BALSA_GEOMETRY_SPATIAL_KDOP_HPP)
#define BALSA_GEOMETRY_SPATIAL_KDOP_HPP

/// @file KDOP.hpp
/// @brief k-DOP (Discrete Oriented Polytope) bounding volume.
///
/// A k-DOP is the intersection of k slab regions, where each slab is
/// defined by a direction vector and a [min, max] interval of projections
/// onto that direction.  AABBs are the special case where k == SpatialDim
/// and the directions are the coordinate axes.
///
/// @tparam T                Scalar type for slab intervals and coordinates.
///                          Must satisfy spatial::ring.  Integral
///                          types are only supported for AABBs
///                          (K == SpatialDim).
/// @tparam SpatialDim       Embedding dimension (e.g. 2 or 3).
/// @tparam K                Number of slab directions (>= SpatialDim).
/// @tparam DirectionPolicy  Policy class providing the direction vectors.
///
/// The direction policy must provide:
/// @code
///   static consteval auto directions()
///       -> std::array<std::array<double, SpatialDim>, K>;
/// @endcode

#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <zipper/Array.hpp>

#include <balsa/geometry/spatial/concepts.hpp>

namespace balsa::geometry::spatial {

// ============================================================================
// DefaultDirections — standard k-DOP direction sets
// ============================================================================

/// Default direction policy for common k-DOP configurations.
///
/// Supported (SpatialDim, K) combinations:
///   - (2, 2): axis-aligned (AABB)
///   - (2, 4): axes + diagonals
///   - (3, 3): axis-aligned (AABB)
///   - (3, 9): axes + face diagonals
///   - (3, 13): axes + face diagonals + space diagonals
template <int8_t SpatialDim, int8_t K>
struct DefaultDirections {
    static_assert(K >= SpatialDim,
                  "k-DOP requires at least SpatialDim slab directions");

    static consteval auto directions()
        -> std::array<std::array<double, SpatialDim>, K>;
};

// ============================================================================
// KDOP — k-DOP bounding volume
// ============================================================================

/// k-DOP (Discrete Oriented Polytope) bounding volume.
///
/// Stores k slab intervals as two ::zipper::Array<T, K> values (min and
/// max projections along each direction).  Supports construction from points,
/// overlap testing, merging, and volume/surface-area estimation.
///
/// @tparam T  Scalar type for slab intervals.
///            Must satisfy spatial::ring.  Integral types are restricted
///            to the AABB case (K == SpatialDim) because general k-DOP
///            slab projections involve floating-point dot products.
///
/// The AABB<SpatialDim, T> alias is provided for the axis-aligned special
/// case.
template <typename T,
          int8_t SpatialDim,
          int8_t K,
          typename DirectionPolicy = DefaultDirections<SpatialDim, K>>
class KDOP {
  public:
    static_assert(K >= SpatialDim,
                  "k-DOP requires at least SpatialDim slab directions");
    static_assert(SpatialDim >= 1, "SpatialDim must be at least 1");
    // ring<T> (additive group + multiplication) suffices for the core
    // KDOP operations: slab projection (dot product), expand, overlap,
    // contains, surface_area, volume, and min_distance_sq — none of
    // which require division.  Division-requiring methods (center,
    // diagonal, ray_intersect) are individually guarded by field<T>.
    static_assert(ring<T>, "KDOP scalar type T must satisfy spatial::ring");
    // k-DOPs with extra directions (K > SpatialDim) project points onto
    // non-axis-aligned directions, which in general requires division
    // (for normalization or inversion).  Axis-aligned bounding boxes
    // (K == SpatialDim) only use coordinate projections, so ring<T>
    // (including integral types like uint32_t) is sufficient.
    static_assert(field<T> || K == SpatialDim,
                  "k-DOPs with K > SpatialDim require spatial::field T "
                  "(integral types are only supported for AABBs)");

    using value_type = T;
    using direction_policy = DirectionPolicy;
    using slab_vector_type = ::zipper::Array<T, K>;

    /// Default-construct an empty k-DOP (inverted intervals).
    KDOP() = default;

    // -- Direction table (compile-time)
    // ----------------------------------------

    /// Returns the k direction vectors as a compile-time array.
    /// Direction vectors are always double-precision regardless of T.
    static consteval auto directions()
        -> std::array<std::array<double, SpatialDim>, K>;

    // -- Construction
    // ----------------------------------------------------------

    /// Returns an empty k-DOP (inverted intervals, ready for expand()).
    [[nodiscard]] static auto empty() -> KDOP;

    /// Returns a k-DOP enclosing a single point.
    [[nodiscard]] static auto from_point(std::span<const T, SpatialDim> point)
        -> KDOP;

    /// Returns a k-DOP tightly fitting a range of points.
    ///
    /// @tparam PointRange  A range whose elements are convertible to
    ///                     std::span<const T, SpatialDim>.
    template <typename PointRange>
    [[nodiscard]] static auto fit(const PointRange &points) -> KDOP;

    // -- Mutation
    // --------------------------------------------------------------

    /// Expands this k-DOP to enclose @p point.
    void expand(std::span<const T, SpatialDim> point);

    /// Inflates this k-DOP by @p margin in every slab direction.
    ///
    /// Each slab interval [lo, hi] becomes [lo - margin, hi + margin].
    /// Useful for proximity queries with floating-point tolerance.
    void inflate(T margin);

    /// Expands this k-DOP to enclose @p other.
    void merge_in_place(const KDOP &other);

    // -- Queries
    // ---------------------------------------------------------------

    /// Returns true if this k-DOP is empty (has inverted intervals).
    ///
    /// An empty k-DOP is one produced by empty() before any expand() or
    /// merge operation.  It has min > max on every slab.
    [[nodiscard]] auto is_empty() const -> bool;

    /// Returns true if @p point lies inside (or on the boundary of) this k-DOP.
    [[nodiscard]] auto contains(std::span<const T, SpatialDim> point) const
        -> bool;

    /// Returns the smallest k-DOP enclosing both this and @p other.
    [[nodiscard]] auto merge(const KDOP &other) const -> KDOP;

    /// Returns true if this k-DOP overlaps @p other.
    [[nodiscard]] auto overlaps(const KDOP &other) const -> bool;

    /// Approximate volume of the bounding region.
    ///
    /// For AABBs (K == SpatialDim) this is exact.  For general k-DOPs this
    /// uses the product of the first SpatialDim slab widths as an
    /// approximation suitable for SAH heuristics.
    [[nodiscard]] auto volume() const -> T;

    /// Approximate surface area (for SAH heuristics).
    ///
    /// For 3D AABBs this is exact.  For general k-DOPs this approximates
    /// using the axis-aligned slab widths.
    [[nodiscard]] auto surface_area() const -> T;

    /// Returns the centroid of the axis-aligned slab midpoints.
    /// Requires field<T> because computing the midpoint (min+max)/2
    /// involves division.
    [[nodiscard]] auto center() const -> std::array<T, SpatialDim>
        requires(field<T>);

    /// Euclidean length of the AABB diagonal (sqrt of sum of squared widths).
    ///
    /// Only available for axis-aligned bounding boxes (K == SpatialDim),
    /// where the slab widths correspond directly to coordinate extents.
    /// Requires field<T> for the same midpoint division as center().
    [[nodiscard]] auto diagonal() const -> T
        requires(K == SpatialDim && field<T>);

    /// Ray-KDOP intersection test.
    ///
    /// Tests whether a ray intersects this k-DOP and returns the entry
    /// and exit t-parameters.  Uses ALL K slab directions for tight
    /// pruning (unlike min_distance_sq which only uses axis-aligned slabs).
    ///
    /// @param origin     Ray origin (SpatialDim coordinates).
    /// @param direction  Ray direction (not necessarily normalized).
    /// @return A pair (t_near, t_far) if the ray intersects the k-DOP,
    ///         or std::nullopt if it misses.  t_near may be negative
    ///         (ray origin is inside the k-DOP).
    [[nodiscard]] auto
        ray_intersect(std::span<const T, SpatialDim> origin,
                      std::span<const T, SpatialDim> direction) const
        -> std::optional<std::pair<T, T>>
        requires(field<T>); // divides by dot(direction, slab_normal)

    /// Squared minimum Euclidean distance from @p point to this k-DOP.
    ///
    /// Returns 0 if the point is inside.  For AABBs (K == SpatialDim)
    /// this is exact.  For general k-DOPs this uses only the first
    /// SpatialDim (axis-aligned) slabs as an approximation, which
    /// always underestimates the true distance — safe for BVH pruning.
    [[nodiscard]] auto
        min_distance_sq(std::span<const T, SpatialDim> point) const -> T;

    // -- Slab accessors
    // --------------------------------------------------------

    /// Min projection along the @p axis-th direction.
    [[nodiscard]] auto min(int8_t axis) const -> T;

    /// Max projection along the @p axis-th direction.
    [[nodiscard]] auto max(int8_t axis) const -> T;

    /// Width of the @p axis-th slab (max - min).
    [[nodiscard]] auto width(int8_t axis) const -> T;

    /// Direct access to the min/max slab vectors.
    [[nodiscard]] auto slab_min() const -> const slab_vector_type &;
    [[nodiscard]] auto slab_max() const -> const slab_vector_type &;

    // -- Comparison
    // ------------------------------------------------------------

    [[nodiscard]] auto operator==(const KDOP &other) const -> bool {
        return (m_min == other.m_min).all() && (m_max == other.m_max).all();
    }

  private:
    slab_vector_type m_min;
    slab_vector_type m_max;

    /// Private constructor from pre-computed slab vectors.
    KDOP(slab_vector_type lo, slab_vector_type hi);

    /// Project a point onto all k directions, returning a slab vector.
    [[nodiscard]] static auto project(std::span<const T, SpatialDim> point)
        -> slab_vector_type;
};

// ============================================================================
// AABB alias
// ============================================================================

/// Axis-Aligned Bounding Box — a k-DOP with k == SpatialDim.
template <typename T, int8_t SpatialDim>
using AABB = KDOP<T, SpatialDim, SpatialDim>;

} // namespace balsa::geometry::spatial

#include "KDOP.hxx"
#endif
