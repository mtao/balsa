#if !defined(BALSA_EIGEN_CONCEPTS_INDEX_TYPES_HPP)
#define BALSA_EIGEN_CONCEPTS_INDEX_TYPES_HPP
#include "shape_types.hpp"

namespace balsa::eigen::concepts {

template<typename T>
concept IntegralMatrixCompatible = MatrixBaseDerived<T> &&std::is_integral_v<typename T::Scalar>;

template<typename T>
concept IntegralVecCompatible = VecCompatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralRowVecCompatible = RowVecCompatible<T> &&std::is_integral_v<typename T::Scalar>;

template<typename T>
concept IntegralVecXCompatible = VecXCompatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralRowVecXCompatible = RowVecXCompatible<T> &&std::is_integral_v<typename T::Scalar>;

template<typename T>
concept IntegralColVecsDCompatible = ColVecsDCompatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralRowVecsDCompatible = RowVecsDCompatible<T> &&std::is_integral_v<typename T::Scalar>;

template<typename T>
concept IntegralColVecs2Compatible = ColVecs2Compatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralRowVecs2Compatible = RowVecs2Compatible<T> &&std::is_integral_v<typename T::Scalar>;

template<typename T>
concept IntegralColVecs3Compatible = ColVecs3Compatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralRowVecs3Compatible = RowVecs3Compatible<T> &&std::is_integral_v<typename T::Scalar>;

template<typename T>
concept IntegralColVecs4Compatible = ColVecs4Compatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralRowVecs4Compatible = RowVecs4Compatible<T> &&std::is_integral_v<typename T::Scalar>;

template<typename T>
concept IntegralVec2Compatible = Vec2Compatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralVec3Compatible = Vec3Compatible<T> &&std::is_integral_v<typename T::Scalar>;
template<typename T>
concept IntegralVec4Compatible = Vec4Compatible<T> &&std::is_integral_v<typename T::Scalar>;


}// namespace balsa::eigen::concepts
#endif
