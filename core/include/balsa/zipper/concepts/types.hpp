#if !defined(BALSA_ZIPPER_CONCEPTS_TYPES_HPP)
#define BALSA_ZIPPER_CONCEPTS_TYPES_HPP

#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>
#include <zipper/concepts/Zipper.hpp>
#include "balsa/zipper/types.hpp"
namespace balsa::zipper::concepts {

template<typename T>
concept MatrixBaseDerived = ::zipper::concepts::Matrix<T>;
template<typename T>
concept ZipperBaseDerived = ::zipper::concepts::Zipper<T>;
template<typename T>
concept VectorBaseDerived = ::zipper::concepts::Vector<T>;
}// namespace balsa::zipper::concepts
#endif
