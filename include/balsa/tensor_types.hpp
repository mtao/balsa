
#if !defined(BALSA_TENSOR_TYPES_HPP)
#define BALSA_TENSOR_TYPES_HPP

#include "zipper/types.hpp"

namespace balsa {
using index_type = zipper::index_type;
using rank_type = zipper::rank_type;
template<typename T, index_type A, index_type B>
using Matrix = zipper::Matrix<T, A, B>;

template<typename T, index_type A>
using SquareMatrix = Matrix<T, A, A>;
template<typename T>
using MatrixX = SquareMatrix<T, std::dynamic_extent>;

template<typename T, index_type D>
using Vector = zipper::Vector<T, D>;
template<typename T>
using VectorX = zipper::VectorX<T>;
template<typename T>
using RowVectorX = zipper::RowVectorX<T>;

template<typename T>
using Vector2 = zipper::Vector2<T>;
template<typename T>
using RowVector2 = zipper::RowVector2<T>;

template<typename T>
using Vector3 = zipper::Vector3<T>;
template<typename T>
using RowVector3 = zipper::RowVector3<T>;

template<typename T>
using Vector4 = zipper::Vector4<T>;
template<typename T>
using RowVector4 = zipper::RowVector4<T>;


template<typename T, index_type D>
using ColVectors = zipper::ColVectors<T,D>;
template<typename T, index_type D>
using RowVectors = zipper::RowVectors<T,D>;

using Mat3i = SquareMatrix<int, 3>;
using Mat2i = SquareMatrix<int, 2>;
using ColVecs4i = ColVectors<int, 4>;
using ColVecs3i = ColVectors<int, 3>;
using ColVecs2i = ColVectors<int, 2>;
using RowVecs3i = RowVectors<int, 3>;
using RowVecs2i = RowVectors<int, 2>;
using Vec3i = Vector<int, 3>;
using Vec2i = Vector<int, 2>;
using MatXi = MatrixX<int>;
using VecXi = VectorX<int>;
using RowVecXi = RowVectorX<int>;

using Mat4f = SquareMatrix<float, 4>;
using Mat3f = SquareMatrix<float, 3>;
using Mat2f = SquareMatrix<float, 2>;
using ColVecs4f = ColVectors<float, 4>;
using ColVecs3f = ColVectors<float, 3>;
using ColVecs2f = ColVectors<float, 2>;
using RowVecs3f = RowVectors<float, 3>;
using RowVecs2f = RowVectors<float, 2>;
using Vec4f = Vector<float, 4>;
using Vec3f = Vector<float, 3>;
using Vec2f = Vector<float, 2>;
using MatXf = MatrixX<float>;
using VecXf = VectorX<float>;
using RowVecXf = RowVectorX<float>;

using Mat4d = SquareMatrix<double, 4>;
using Mat3d = SquareMatrix<double, 3>;
using Mat2d = SquareMatrix<double, 2>;
using ColVecs4d = ColVectors<double, 4>;
using ColVecs3d = ColVectors<double, 3>;
using ColVecs2d = ColVectors<double, 2>;
using RowVecs3d = RowVectors<double, 3>;
using RowVecs2d = RowVectors<double, 2>;
using Vec4d = Vector<double, 4>;
using Vec3d = Vector<double, 3>;
using Vec2d = Vector<double, 2>;
using MatXd = MatrixX<double>;
using VecXd = VectorX<double>;
using RowVecXd = RowVectorX<double>;

using Vec2i = Vector2<int>;
using Vec3i = Vector3<int>;
using Vec4i = Vector4<int>;

}// namespace balsazipper

#endif
