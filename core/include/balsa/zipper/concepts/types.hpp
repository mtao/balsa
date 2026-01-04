#if !defined(BALSA_ZIPPER_CONCEPTS_TYPES_HPP)
#define BALSA_ZIPPER_CONCEPTS_TYPES_HPP

#include <zipper/concepts/MatrixBaseDerived.hpp>
#include <zipper/concepts/VectorBaseDerived.hpp>
#include <zipper/concepts/ZipperBaseDerived.hpp>
#include "balsa/zipper/types.hpp"
namespace balsa::zipper::concepts {

template<typename T>
concept MatrixBaseDerived = ::zipper::concepts::MatrixBaseDerived<T>;
template<typename T>
concept ZipperBaseDerived = ::zipper::concepts::ZipperBaseDerived<T>;
template<typename T>
concept VectorBaseDerived = ::zipper::concepts::VectorBaseDerived<T>;
}// namespace balsa::zipper::concepts
#endif
