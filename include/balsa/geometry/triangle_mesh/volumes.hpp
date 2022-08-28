#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_VOLUMES_HPP)
#define BALSA_GEOMETRY_TRIANGLE_MESH_VOLUMES_HPP

#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/geometry/simplex/volume.hpp"
#include "balsa/eigen/concepts/index_types.hpp"
#include "balsa/utils/factorial.hpp"
namespace balsa::geometry::triangle_mesh {
template<eigen::concepts::RowStaticCompatible VertexDerived, eigen::concepts::RowStaticCompatible SimplexDerived>
requires(eigen::concepts::IntegralMatrix<SimplexDerived>) auto volumes(const VertexDerived &V,
                                                                       const SimplexDerived &S) {
    // constexpr static int E = eigen::concepts::detail::compile_row_size<VertexDerived>;// embed dim
    //  constexpr static int N = SimplexDerived::ColsAtCompileTime;//number of
    //  elements
    // constexpr static int D = eigen::concepts::detail::compile_row_size<VertexDerived>;// simplex dim
    constexpr static int N = eigen::concepts::detail::compile_col_size<SimplexDerived>;// simplex dim
    using Scalar = typename VertexDerived::Scalar;

    balsa::eigen::Vector<Scalar, N> C(S.cols());

    // tbb::parallel_for(int(0), int(S.cols()), [&](int j) {
    //     auto s = S.col(j);
    //     C(j) = volume(V(Eigen::all, s));
    // });
    for (int j = 0; j < S.cols(); ++j) {
        auto s = S.col(j);
        C(j) = simplex::volume(V(Eigen::all, s));
    }
    return C;
}
}// namespace balsa::geometry::triangle_mesh

// TODO find homes for these functions
// template<typename VertexDerived, eigen::concepts::IntegralMatrix SimplexDerived>
// auto brep_volume(const VertexDerived &V,
//                 const SimplexDerived &S) {
//    constexpr static int E = VertexDerived::RowsAtCompileTime;// embed dim
//    // constexpr static int N = SimplexDerived::ColsAtCompileTime;//number of
//    // elements
//    constexpr static int D = SimplexDerived::RowsAtCompileTime;// simplex dim
//    using Scalar = typename VertexDerived::Scalar;
//
//    balsa::eigen::Matrix<Scalar, E, (D != Eigen::Dynamic) ? (D + 1) : Eigen::Dynamic> v(
//      V.rows(), S.rows() + 1);
//    v.col(D).setZero();
//    Scalar myvol = 0;
//    for (int i = 0; i < S.cols(); ++i) {
//        auto s = S.col(i);
//        for (int j = 0; j < s.rows(); ++j) {
//            v.col(j) = V.col(s(j));
//        }
//        myvol += volume(v);
//    }
//    return myvol;
//}
//
// template<typename VertexDerived, eigen::concepts::IntegralMatrix SimplexDerived>
// auto dual_volumes(const VertexDerived &V,
//                  const SimplexDerived &S) {
//    auto PV = volumes(V, S);
//    int elementsPerCell = S.rows();
//    using Scalar = typename VertexDerived::Scalar;
//    balsa::eigen::VectorX<Scalar> Vo = balsa::eigen::VectorX<Scalar>::Zero(V.cols());
//    for (int i = 0; i < S.cols(); ++i) {
//        auto s = S.col(i);
//        auto v = PV(i);
//        for (int j = 0; j < S.rows(); ++j) {
//            Vo(s(j)) += v;
//        }
//    }
//    Vo /= elementsPerCell;
//    return Vo;
//}
//
// template<int D, int S>
// struct dim_specific {};
// template<int S>
// struct dim_specific<2, S> {
//    template<typename Derived>
//    static auto convex_volume(const Eigen::MatrixBase<Derived> &V) ->
//      typename Derived::Scalar {
//        using Scalar = typename Derived::Scalar;
//        balsa::eigen::Matrix<Scalar, Derived::RowsAtCompileTime, S + 1> M(V.rows(),
//                                                                          S + 1);
//
//        if constexpr (S == 0) {
//            return 1;
//        } else {
//            if (V.cols() < S + 1) {
//                return 0;
//            } else {
//                Scalar r = 0;
//                M.col(0) = V.col(0);
//
//                for (int j = 1; j < V.cols() - S + 1; ++j) {
//                    for (int k = 0; k < S; ++k) {
//                        M.col(k + 1) = V.col(j + k);
//                    }
//                    r += volume_unsigned(M);
//                }
//                return r;
//            }
//        }
//    }
//};
//
// template<eigen::concepts::VecCompatible ADerived, eigen::concepts::VecCompatible BDerived, eigen::concepts::VecCompatible CDerived>
// typename ADerived::Scalar triangle_area(const Eigen::MatrixBase<ADerived> &a,
//                                        const Eigen::MatrixBase<BDerived> &b,
//                                        const Eigen::MatrixBase<CDerived> &c) {
//    if constexpr (ADerived::RowsAtCompileTime == 2) {
//        return .5 * (b - a).homogeneous().cross((c - a).homogeneous()).z();
//    } else if constexpr (ADerived::RowsAtCompileTime == 3) {
//        return .5 * (b - a).cross(c - a).norm();
//    } else if constexpr (ADerived::RowsAtCompileTime == Eigen::Dynamic) {
//        if (a.rows() == 2) {
//            return .5 * (b - a).homogeneous().cross((c - a).homogeneous()).z();
//        } else if (a.rows() == 3) {
//            return .5 * (b - a).cross(c - a).norm();
//        }
//    }
//    // fall through is to use heron's formula
//    auto l0 = (b - a).norm();
//    auto l1 = (b - c).norm();
//    auto l2 = (c - a).norm();
//    // auto s = (l0 + l1 + l2) / 2;
//    return std::sqrt((l0 + l1 + l2) * (2 * l0 + 2 * l1 - l2) * (2 * l0 - l1 + 2 * l2) * (-l0 + 2 * l1 + 2 * l2)) / 4;
//}
//
// template<eigen::concepts::Vec3Compatible ADerived, eigen::concepts::Vec3Compatible BDerived, eigen::concepts::Vec3Compatible CDerived, eigen::concepts::Vec3Compatible DDerived>
// typename ADerived::Scalar tetrahedron_volume(
//  const ADerived &a,
//  const BDerived &b,
//  const CDerived &c,
//  const DDerived &d) {
//    return (b - a).cross(c - a).dot(d - a) / 6.;
//}
//
//// TODO: Check the orientation of these
// template<eigen::concepts::VecCompatible ADerived, eigen::concepts::VecCompatible BDerived>
// typename ADerived::Scalar triangle_area(const ADerived &a,
//                                         const BDerived &b) {
//     return triangle_area(ADerived::Zero(a.size()), a, b);
// }
// template<eigen::concepts::Vec3Compatible ADerived, eigen::concepts::Vec3Compatible BDerived, eigen::concepts::Vec3Compatible CDerived>
// typename ADerived::Scalar tetrahedron_volume(
//   const ADerived &a,
//   const BDerived &b,
//   const CDerived &c) {
//     return tetrahedron_volume(ADerived::Zero(a.size()), a, b, c);
// }
//

#endif
