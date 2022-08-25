#if !defined(BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP
#include <cassert>
#include <numeric>
#include <stdexcept>
#include <tbb/parallel_for.h>
#include <fmt/format.h>

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/shape_checks.hpp"
#include "balsa/eigen/concepts/index_types.hpp"
#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/utils/factorial.hpp"

namespace balsa::geometry::simplex {

namespace detail::volume {
    template<int RowsMinusCols, eigen::concepts::MatrixBaseDerived Mat>
    constexpr std::array<int, 2> relative_shape() {
        using namespace Eigen;
        constexpr int Rows = Mat::RowsAtCompileTime;
        constexpr int Cols = Mat::ColsAtCompileTime;
        // Rows = Cols + RowsMinusCols
        if constexpr (Rows != Dynamic) {
            if constexpr (Cols != Dynamic) {
                static_assert(Rows == Cols + RowsMinusCols);

                return std::array<int, 2>{ { Rows, Cols } };

            } else {
                // have rows but not cols
                // Rows - RowsMinusCols = Cols
                return std::array<int, 2>{ { Rows, Rows - RowsMinusCols } };
            }
        } else {
            // Rows = Cols + RowsMinusCols
            if constexpr (Cols != Dynamic) {
                return std::array<int, 2>{ { Cols + RowsMinusCols, Cols } };
            } else {
                return std::array<int, 2>{ { Eigen::Dynamic, Eigen::Dynamic } };
            }
        }
    }
    template<int RowsMinusCols, eigen::concepts::MatrixBaseDerived Mat>
    constexpr int relative_rows() {
        return relative_shape<RowsMinusCols, Mat>()[0];
    }
    template<int RowsMinusCols, eigen::concepts::MatrixBaseDerived Mat>
    constexpr int relative_cols() {
        return relative_shape<RowsMinusCols, Mat>()[1];
    }
    namespace concepts {

        template<typename T>
        concept OneMoreColThanRows =
          relative_shape<-1, T>() == std::array<int, 2>{ { Eigen::Dynamic, Eigen::Dynamic } } || relative_rows<-1, T>() + 1 == relative_cols<-1, T>();

    }


}// namespace detail::volume


template<detail::volume::concepts::OneMoreColThanRows Derived>
auto volume_signed(const Derived &V) {

    if constexpr (detail::volume::relative_shape<-1, Derived>() == std::array<int, 2>{ { Eigen::Dynamic, Eigen::Dynamic } }) {
        if (V.cols() == V.rows() + 1) {

            throw std::invalid_argument(fmt::format("volume_signed expected a N,N+1 matrix, got got {} expected {}", V.rows(), V.cols()));
        }
    }
    auto m = (V.rightCols(V.cols() - 1).colwise() - V.col(0)).eval();
    return m.determinant() / balsa::utils::factorial(m.cols());
}

template<typename Derived>
auto volume_unsigned(const Eigen::MatrixBase<Derived> &V) {
    auto m = (V.rightCols(V.cols() - 1).colwise() - V.col(0)).eval();
    int quotient = balsa::utils::factorial(m.cols());
    return std::sqrt((m.transpose() * m).determinant()) / quotient;
}

template<eigen::concepts::PlainObjectBaseDerived MatType>
auto volume(const MatType &V) -> typename MatType::Scalar {
    if constexpr(eigen::concepts::RowColStaticCompatible<MatType>)
    {
    }
    if (V.rows() + 1 == V.cols()) {
        return volume_signed(V);
    } else {
        return volume_unsigned(V);
    }
}

template<eigen::concepts::MatrixBaseDerived VertexDerived, eigen::concepts::IntegralMatrix SimplexDerived>
auto volumes(const VertexDerived &V,
                     const SimplexDerived &S) {
    constexpr static int E = VertexDerived::RowsAtCompileTime;// embed dim
    // constexpr static int N = SimplexDerived::ColsAtCompileTime;//number of
    // elements
    constexpr static int D = SimplexDerived::RowsAtCompileTime;// simplex dim
    using Scalar = typename VertexDerived::Scalar;

    balsa::eigen::VectorX<Scalar> C(S.cols());

    tbb::parallel_for(int(0), int(S.cols()), [&](int i) {
        auto s = S.col(i);
        balsa::eigen::Matrix<Scalar, E, D> v(V.rows(), S.rows());
        for (int j = 0; j < s.rows(); ++j) {
            v.col(j) = V.col(s(j));
        }
        C(i) = volume(v);
    });
    return C;
}

template<typename VertexDerived, eigen::concepts::IntegralMatrix SimplexDerived>
auto brep_volume(const VertexDerived &V,
                 const SimplexDerived &S) {
    constexpr static int E = VertexDerived::RowsAtCompileTime;// embed dim
    // constexpr static int N = SimplexDerived::ColsAtCompileTime;//number of
    // elements
    constexpr static int D = SimplexDerived::RowsAtCompileTime;// simplex dim
    using Scalar = typename VertexDerived::Scalar;

    balsa::eigen::Matrix<Scalar, E, (D != Eigen::Dynamic) ? (D + 1) : Eigen::Dynamic> v(
      V.rows(), S.rows() + 1);
    v.col(D).setZero();
    Scalar myvol = 0;
    for (int i = 0; i < S.cols(); ++i) {
        auto s = S.col(i);
        for (int j = 0; j < s.rows(); ++j) {
            v.col(j) = V.col(s(j));
        }
        myvol += volume(v);
    }
    return myvol;
}

template<typename VertexDerived, eigen::concepts::IntegralMatrix SimplexDerived>
auto dual_volumes(const VertexDerived &V,
                  const SimplexDerived &S) {
    auto PV = volumes(V, S);
    int elementsPerCell = S.rows();
    using Scalar = typename VertexDerived::Scalar;
    balsa::eigen::VectorX<Scalar> Vo = balsa::eigen::VectorX<Scalar>::Zero(V.cols());
    for (int i = 0; i < S.cols(); ++i) {
        auto s = S.col(i);
        auto v = PV(i);
        for (int j = 0; j < S.rows(); ++j) {
            Vo(s(j)) += v;
        }
    }
    Vo /= elementsPerCell;
    return Vo;
}

template<int D, int S>
struct dim_specific {};
template<int S>
struct dim_specific<2, S> {
    template<typename Derived>
    static auto convex_volume(const Eigen::MatrixBase<Derived> &V) ->
      typename Derived::Scalar {
        using Scalar = typename Derived::Scalar;
        balsa::eigen::Matrix<Scalar, Derived::RowsAtCompileTime, S + 1> M(V.rows(),
                                                                          S + 1);

        if constexpr (S == 0) {
            return 1;
        } else {
            if (V.cols() < S + 1) {
                return 0;
            } else {
                Scalar r = 0;
                M.col(0) = V.col(0);

                for (int j = 1; j < V.cols() - S + 1; ++j) {
                    for (int k = 0; k < S; ++k) {
                        M.col(k + 1) = V.col(j + k);
                    }
                    r += volume_unsigned(M);
                }
                return r;
            }
        }
    }
};

template<eigen::concepts::VecCompatible ADerived, eigen::concepts::VecCompatible BDerived, eigen::concepts::VecCompatible CDerived>
typename ADerived::Scalar triangle_area(const Eigen::MatrixBase<ADerived> &a,
                                        const Eigen::MatrixBase<BDerived> &b,
                                        const Eigen::MatrixBase<CDerived> &c) {
    if constexpr (ADerived::RowsAtCompileTime == 2) {
        return .5 * (b - a).homogeneous().cross((c - a).homogeneous()).z();
    } else if constexpr (ADerived::RowsAtCompileTime == 3) {
        return .5 * (b - a).cross(c - a).norm();
    } else if constexpr (ADerived::RowsAtCompileTime == Eigen::Dynamic) {
        if (a.rows() == 2) {
            return .5 * (b - a).homogeneous().cross((c - a).homogeneous()).z();
        } else if (a.rows() == 3) {
            return .5 * (b - a).cross(c - a).norm();
        }
    }
    // fall through is to use heron's formula
    auto l0 = (b - a).norm();
    auto l1 = (b - c).norm();
    auto l2 = (c - a).norm();
    // auto s = (l0 + l1 + l2) / 2;
    return std::sqrt((l0 + l1 + l2) * (2 * l0 + 2 * l1 - l2) * (2 * l0 - l1 + 2 * l2) * (-l0 + 2 * l1 + 2 * l2)) / 4;
}

template<eigen::concepts::Vec3Compatible ADerived, eigen::concepts::Vec3Compatible BDerived, eigen::concepts::Vec3Compatible CDerived, eigen::concepts::Vec3Compatible DDerived>
typename ADerived::Scalar tetrahedron_volume(
  const ADerived &a,
  const BDerived &b,
  const CDerived &c,
  const DDerived &d) {
    return (b - a).cross(c - a).dot(d - a) / 6.;
}

// TODO: Check the orientation of these
template<eigen::concepts::VecCompatible ADerived, eigen::concepts::VecCompatible BDerived>
typename ADerived::Scalar triangle_area(const ADerived &a,
                                        const BDerived &b) {
    return triangle_area(ADerived::Zero(a.size()), a, b);
}
template<eigen::concepts::Vec3Compatible ADerived, eigen::concepts::Vec3Compatible BDerived, eigen::concepts::Vec3Compatible CDerived>
typename ADerived::Scalar tetrahedron_volume(
  const ADerived &a,
  const BDerived &b,
  const CDerived &c) {
    return tetrahedron_volume(ADerived::Zero(a.size()), a, b, c);
}
}// namespace balsa::geometry
#endif// VOLUME_H
