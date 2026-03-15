#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_VOLUMES_HPP)
#define BALSA_GEOMETRY_TRIANGLE_MESH_VOLUMES_HPP

#include "balsa/geometry/simplicial_complex/volumes.hpp"
#include "balsa/utils/factorial.hpp"
namespace balsa::geometry::triangle_mesh {
template<
  ::zipper::concepts::MatrixBaseDerived VertexDerived,
  ::zipper::concepts::MatrixBaseDerived SimplexDerived>
    requires(std::is_integral_v<typename SimplexDerived::value_type>)
auto volumes(const VertexDerived &V,
             const SimplexDerived &S) {
    return simplicial_complex::volumes(V, S);
}
}// namespace balsa::geometry::triangle_mesh

#endif
