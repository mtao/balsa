#pragma once
// Bridge utilities between quiver's IncidentData and zipper's Matrix types.
//
// The key conversion is:
//   IncidentData<std::array<Scalar, D>, N, Storage> --> Matrix<Scalar, D, N>
//
// This enables quiver's per-simplex attribute gathering to feed directly
// into balsa's simplex kernels (volume, circumcenter, etc.).

#include <quiver/attributes/IncidentData.hpp>
#include <quiver/topology/SimplexIndices.hpp>
#include "balsa/zipper/types.hpp"
#include <array>
#include <cstddef>
#include <type_traits>

namespace balsa::geometry::quiver {

// ── Concepts ──────────────────────────────────────────────────────────────

namespace detail {

    // Detect std::array<T, N>.
    template<typename T>
    struct is_std_array : std::false_type {};

    template<typename T, size_t N>
    struct is_std_array<std::array<T, N>> : std::true_type {};

    // Extract element type from std::array<T, N>.
    template<typename A>
    struct array_traits;

    template<typename T, size_t N>
    struct array_traits<std::array<T, N>> {
        using value_type = T;
        static constexpr size_t size = N;
    };

}// namespace detail

/// Concept: an IncidentData whose value_type is std::array<Scalar, D>.
template<typename ID>
concept ArrayIncidentData =
  requires {
      typename ID::value_type;
  } && detail::is_std_array<typename ID::value_type>::value;


// ── to_col_vectors ────────────────────────────────────────────────────────

/// Convert an IncidentData<array<Scalar, D>, N, Storage> to a
/// Matrix<Scalar, D, N>.  Copies scattered incident data into a
/// column-major matrix suitable for simplex kernels.
///
/// For fixed-extent IncidentData, both D and N are compile-time constants.
/// For dynamic-extent IncidentData, N is std::dynamic_extent and the
/// matrix has dynamic columns.
template<ArrayIncidentData IDType>
auto to_col_vectors(const IDType &id) {
    using array_type = typename IDType::value_type;
    using Scalar = typename detail::array_traits<array_type>::value_type;
    constexpr size_t D = detail::array_traits<array_type>::size;
    constexpr size_t N = IDType::extent;

    // D rows x N columns (N may be dynamic_extent)
    ::zipper::Matrix<Scalar, D, N> result(D, id.size());

    for (size_t j = 0; j < id.size(); ++j) {
        const auto &arr = id[j];
        for (size_t i = 0; i < D; ++i) {
            result(i, j) = arr[i];
        }
    }
    return result;
}


// ── to_std_array ──────────────────────────────────────────────────────────

/// Convert a zipper Vector<Scalar, N> to std::array<Scalar, N>.
/// Used to convert simplex kernel results back to quiver-compatible types.
template<::zipper::concepts::Zipper VecType>
    requires(VecType::extents_type::rank() == 1 && !VecType::extents_traits::is_dynamic)
auto to_std_array(const VecType &v) {
    using Scalar = typename VecType::value_type;
    constexpr size_t N = VecType::static_extent(0);
    std::array<Scalar, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = v(i);
    }
    return result;
}


// ── make_incident_data ────────────────────────────────────────────────────

/// Construct a referencing IncidentData from a raw pointer and index array.
///
/// This is the lightweight path -- no Mesh or Skeleton needed.  The caller
/// provides a contiguous buffer of T values and an array of indices into
/// that buffer.
///
/// @tparam T  Element type (typically std::array<double, D>).
/// @tparam N  Number of incident elements (e.g. 3 for a triangle).
/// @param base     Pointer to the contiguous attribute storage.
/// @param indices  Array of indices into base.
template<typename T, size_t N>
auto make_incident_data(
  const T *base,
  std::array<int64_t, N> indices) {
    return ::quiver::attributes::IncidentData<
      T,
      N,
      ::quiver::attributes::ReferencingStorage>(base, indices);
}

/// Construct an owning IncidentData from materialized values and indices.
template<typename T, size_t N>
auto make_incident_data(
  std::array<T, N> values,
  std::array<int64_t, N> indices) {
    return ::quiver::attributes::IncidentData<
      T,
      N,
      ::quiver::attributes::OwningStorage>(
      std::move(values), std::move(indices));
}


// ── to_col_vectors from SimplexIndices + positions ────────────────────────

/// Extract the vertex positions for the j-th simplex directly from
/// a SimplexIndices and a position buffer.
///
/// Lightweight path: no Mesh needed.  Returns a Matrix<Scalar, D, K+1>
/// where K = SimplexDim (e.g. 3 vertices for a triangle, 4 for a tet).
///
/// @tparam SimplexDim  Sub-face dimension (0 for vertex indices).
/// @tparam MeshDim     Top-level simplex dimension.
/// @tparam Scalar      Coordinate scalar type.
/// @tparam D           Spatial dimension.
/// @param si           SimplexIndices containing per-simplex vertex arrays.
/// @param positions    Pointer to contiguous array<Scalar, D> buffer.
/// @param simplex_idx  Which simplex to extract.
template<int8_t SimplexDim, int8_t MeshDim, typename Scalar, size_t D>
auto simplex_col_vectors(
  const ::quiver::topology::SimplexIndices<SimplexDim, MeshDim> &si,
  const std::array<Scalar, D> *positions,
  size_t simplex_idx) {

    // Get the index array for this simplex
    const auto &idx_array = si.handle()[simplex_idx];

    auto id = make_incident_data(positions, idx_array);
    return to_col_vectors(id);
}


}// namespace balsa::geometry::quiver
