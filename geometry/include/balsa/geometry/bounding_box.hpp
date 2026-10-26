#if !defined(BALSA_GEOMETRY_BOUNDING_BOX)
#define BALSA_GEOMETRY_BOUNDING_BOX
#include "BoundingBox.hpp"
#include <zipper/Matrix.hpp>

namespace balsa::geometry {

template <::zipper::concepts::Matrix VType>
auto bounding_box(const VType &V) {
    using T = typename VType::value_type;
    constexpr auto Dim = VType::extents_type::static_extent(0);
    if (V.extent(1) == 0) {
        return BoundingBox<T, Dim>{};
    }
    return BoundingBox<T, Dim>(V.rowwise().min(), V.rowwise().max());
}

} // namespace balsa::geometry

#endif
