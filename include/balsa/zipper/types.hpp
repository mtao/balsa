#if !defined(BALSA_ZIPPER_TYPES_HPP)
#define BALSA_ZIPPER_TYPES_HPP

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/Form.hpp>
#include <zipper/Tensor.hpp>

namespace balsa::zipper {
using index_type = ::zipper::index_type;
template<typename T, index_type A, index_type B>
using Matrix = ::zipper::Matrix<T, A, B>;

template<typename T, index_type A>
using SquareMatrix = Matrix<T, A, A>;
template<typename T>
using MatrixX = SquareMatrix<T, std::dynamic_extent>;

template<typename T, index_type D>
using Vector = ::zipper::Vector<T, D>;
template<typename T, index_type D>
using RowVector = ::zipper::Form<T, D>;
template<typename T>
using VectorX = Vector<T, std::dynamic_extent>;
template<typename T>
using RowVectorX = RowVector<T, std::dynamic_extent>;

template<typename T>
using Vector2 = Vector<T, 2>;
template<typename T>
using RowVector2 = RowVector<T, 2>;

template<typename T>
using Vector3 = Vector<T, 3>;
template<typename T>
using RowVector3 = RowVector<T, 3>;

template<typename T>
using Vector4 = Vector<T, 4>;
template<typename T>
using RowVector4 = RowVector<T, 4>;

template<typename T, index_type D>
using ColVectors = Matrix<T, D, std::dynamic_extent>;
template<typename T, index_type D>
using RowVectors = Matrix<T, std::dynamic_extent, D>;

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

}// namespace balsa::zipper

#endif
