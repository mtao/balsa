#pragma once
// Transform functors that compose the quiver -> zipper bridge with
// simplex kernels.  These are designed to be used with quiver's pipe
// system:
//
//   pipe(mesh, pos) | as_dart<0>() | gather_faces<2>() | transform(VolumeTransform{})
//
// Or directly with make_incident_data:
//
//   auto id = make_incident_data(pos_ptr, indices);
//   auto vol = VolumeTransform{}(id);

#include "to_col_vectors.hpp"
#include "balsa/geometry/simplex/volume.hpp"
#include "balsa/geometry/simplex/circumcenter.hpp"

namespace balsa::geometry::quiver {

/// Functor: IncidentData -> scalar volume.
///
/// Computes the volume of the simplex whose vertices are gathered in
/// the IncidentData.  For D-by-(D+1) simplices the result is signed;
/// otherwise unsigned.
struct VolumeTransform {
    template<ArrayIncidentData IDType>
    auto operator()(const IDType &id) const {
        auto V = to_col_vectors(id);
        return simplex::volume(V);
    }
};


/// Functor: IncidentData -> std::array<Scalar, D> circumcenter.
///
/// Computes the circumcenter of the simplex.  Uses the SPD solver
/// when D+1 == N (full-rank), and falls back to SPSD (QR) otherwise.
struct CircumcenterTransform {
    template<ArrayIncidentData IDType>
    auto operator()(const IDType &id) const {
        auto V = to_col_vectors(id);
        auto C = simplex::circumcenter(V);
        return to_std_array(C);
    }
};


/// Functor: IncidentData -> pair<array<Scalar, D>, Scalar>.
///
/// Returns both the circumcenter and the squared circumradius.
struct CircumcenterWithSquaredRadiusTransform {
    template<ArrayIncidentData IDType>
    auto operator()(const IDType &id) const {
        auto V = to_col_vectors(id);
        auto [C, r2] = simplex::circumcenter_with_squared_radius(V);
        return std::make_pair(to_std_array(C), r2);
    }
};


/// Functor: IncidentData -> pair<array<Scalar, D>, Scalar>.
///
/// Returns both the circumcenter and the circumradius.
struct CircumcenterWithRadiusTransform {
    template<ArrayIncidentData IDType>
    auto operator()(const IDType &id) const {
        auto V = to_col_vectors(id);
        auto [C, r] = simplex::circumcenter_with_radius(V);
        return std::make_pair(to_std_array(C), r);
    }
};


}// namespace balsa::geometry::quiver
