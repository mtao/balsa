#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_EARCLIPPING_HPP)
#define BALSA_GEOMETRY_TRIANGLE_MESH_EARCLIPPING_HPP
#include <list>

#include <algorithm>
#include <ranges>
#include "balsa/eigen/stl2eigen.hpp"
#include <Eigen/Core>
#include "balsa/geometry/winding_number.hpp"
#include "balsa/geometry/get_angle.hpp"
#include "balsa/zipper/concepts/shape_types.hpp"
#include <numbers>
#include <utility>
#include <zipper/MatrixBase.hpp>
#include <zipper/Vector.hpp>
#include <zipper/storage/StlStorageInfo.hpp>

namespace balsa::geometry::triangle_mesh {

// ════════════════════════════════════════════════════════════════════════════
// Eigen earclipping_stl
// ════════════════════════════════════════════════════════════════════════════

template<eigen::concepts::Vec2Compatible VDerived, typename IndexType>
    requires std::is_integral_v<IndexType>
std::vector<std::array<IndexType, 3>> earclipping_stl(
  const VDerived &V,
  std::span<const IndexType> indices) {
    using Index = IndexType;
    using Face = std::array<Index, 3>;
    std::vector<Face> stlF;


    size_t size = indices.size();
    stlF.reserve(size - 2);
    double inner_ang_sum = 0;
    double outer_ang_sum = 0;
    for (size_t i = 0; i < size; ++i) {
        Face f{ { indices[i], indices[(i + 1) % size], indices[(i + 2) % size] } };
        const auto &[ai, bi, ci] = f;
        auto a = V.col(ai);
        auto b = V.col(bi);
        auto c = V.col(ci);
        double ang = get_positive_clamped_angle(get_angle(c - b, a - b));
        inner_ang_sum += ang;
        outer_ang_sum += 2 * std::numbers::pi_v<double> - ang;
    }

    bool reverse_orientation = outer_ang_sum < inner_ang_sum;
    if (reverse_orientation) {
    }

    std::list<Index> CL(indices.begin(), indices.end());

    auto is_earclip = [&](const Face &f) -> bool {
        auto a = V.col(f[0]);
        auto b = V.col(f[1]);
        auto c = V.col(f[2]);
        auto cb = c - b;
        auto ab = a - b;
        if (cb.x() * ab.y() - cb.y() * ab.x() < 1e-10) {
            return false;
        }
        double ang = get_positive_clamped_angle(get_angle(c - b, a - b));
        if (ang > std::numbers::pi_v<double> || ang < 0) {
            return false;
        }

        for (auto &&i : indices) {
            if (i == f[0] || i == f[1] || i == f[2]) {
                continue;
            }
            auto v = V.col(i);
            if (interior_winding_number(V, f, v)) {
                return false;
            }
        }
        return true;
    };

    while (CL.size() > 3) {
        bool earclipped = false;

        for (auto it = CL.begin(); it != CL.end(); ++it) {
            auto it1 = it;
            it1++;
            if (it1 == CL.end()) {
                it1 = CL.begin();
            }
            auto it2 = it1;
            it2++;
            if (it2 == CL.end()) {
                it2 = CL.begin();
            }
            Face f{ { *it, *it1, *it2 } };
            if (reverse_orientation) {
                std::swap(f[0], f[2]);
            }
            if (is_earclip(f)) {
                stlF.push_back(f);
                CL.erase(it1);
                earclipped = true;
                break;
            }
        }
        if (!earclipped) {
            auto it = CL.begin();
            auto it1 = it;
            it1++;
            auto it2 = it1;
            it2++;
            Face f{ { *it, *it1, *it2 } };


            stlF.push_back(f);
            CL.erase(it1);
        }
    }

    Face f;
    std::copy(CL.begin(), CL.end(), f.begin());
    auto a = V.col(f[0]);
    auto b = V.col(f[1]);
    auto c = V.col(f[2]);
    auto cb = c - b;
    auto ab = a - b;
    auto cross = cb.x() * ab.y() - cb.y() * ab.x();
    if (cross < -1e-10) {
        std::swap(f[0], f[1]);
    } else if (std::abs(cross) <= 1e-10) {
        // degenerate (collinear) final triangle — skip it
        return stlF;
    }
    stlF.push_back(f);
    return stlF;
}

// ════════════════════════════════════════════════════════════════════════════
// Zipper earclipping_stl — native, no Eigen roundtrip
// ════════════════════════════════════════════════════════════════════════════

template<zipper::concepts::ColVecs2Compatible VDerived, typename IndexType>
    requires std::is_integral_v<IndexType>
std::vector<std::array<IndexType, 3>> earclipping_stl(
  const VDerived &V,
  std::span<const IndexType> indices) {
    using Index = IndexType;
    using Face = std::array<Index, 3>;
    using S = typename VDerived::value_type;
    std::vector<Face> stlF;

    size_t size = indices.size();
    stlF.reserve(size - 2);
    double inner_ang_sum = 0;
    double outer_ang_sum = 0;
    for (size_t i = 0; i < size; ++i) {
        Face f{ { indices[i], indices[(i + 1) % size], indices[(i + 2) % size] } };
        const auto &[ai, bi, ci] = f;
        auto a = V.col(std::size_t(ai));
        auto b = V.col(std::size_t(bi));
        auto c = V.col(std::size_t(ci));
        ::zipper::Vector<S, 2> cb = c - b;
        ::zipper::Vector<S, 2> ab = a - b;
        double ang = get_positive_clamped_angle(get_angle(cb, ab));
        inner_ang_sum += ang;
        outer_ang_sum += 2 * std::numbers::pi_v<double> - ang;
    }

    bool reverse_orientation = outer_ang_sum < inner_ang_sum;
    if (reverse_orientation) {
    }

    std::list<Index> CL(indices.begin(), indices.end());

    auto is_earclip = [&](const Face &f) -> bool {
        auto a = V.col(std::size_t(f[0]));
        auto b = V.col(std::size_t(f[1]));
        auto c = V.col(std::size_t(f[2]));
        ::zipper::Vector<S, 2> cb = c - b;
        ::zipper::Vector<S, 2> ab = a - b;
        // 2D cross product: cb.x * ab.y - cb.y * ab.x
        if (cb(0) * ab(1) - cb(1) * ab(0) < 1e-10) {
            return false;
        }
        double ang = get_positive_clamped_angle(get_angle(cb, ab));
        if (ang > std::numbers::pi_v<double> || ang < 0) {
            return false;
        }

        for (auto &&i : indices) {
            if (i == f[0] || i == f[1] || i == f[2]) {
                continue;
            }
            auto v = V.col(std::size_t(i));
            if (interior_winding_number(V, f, v)) {
                return false;
            }
        }
        return true;
    };

    while (CL.size() > 3) {
        bool earclipped = false;

        for (auto it = CL.begin(); it != CL.end(); ++it) {
            auto it1 = it;
            it1++;
            if (it1 == CL.end()) {
                it1 = CL.begin();
            }
            auto it2 = it1;
            it2++;
            if (it2 == CL.end()) {
                it2 = CL.begin();
            }
            Face f{ { *it, *it1, *it2 } };
            if (reverse_orientation) {
                std::swap(f[0], f[2]);
            }
            if (is_earclip(f)) {
                stlF.push_back(f);
                CL.erase(it1);
                earclipped = true;
                break;
            }
        }
        if (!earclipped) {
            auto it = CL.begin();
            auto it1 = it;
            it1++;
            auto it2 = it1;
            it2++;
            Face f{ { *it, *it1, *it2 } };


            stlF.push_back(f);
            CL.erase(it1);
        }
    }

    Face f;
    std::copy(CL.begin(), CL.end(), f.begin());
    auto a = V.col(std::size_t(f[0]));
    auto b = V.col(std::size_t(f[1]));
    auto c = V.col(std::size_t(f[2]));
    ::zipper::Vector<S, 2> cb = c - b;
    ::zipper::Vector<S, 2> ab = a - b;
    auto cross = cb(0) * ab(1) - cb(1) * ab(0);
    if (cross < -1e-10) {
        std::swap(f[0], f[1]);
    } else if (std::abs(cross) <= 1e-10) {
        // degenerate (collinear) final triangle — skip it
        return stlF;
    }
    stlF.push_back(f);
    return stlF;
}

// ════════════════════════════════════════════════════════════════════════════
// Public API — Eigen
// ════════════════════════════════════════════════════════════════════════════

// returns balsa::eigen::ColVectors<integral_type, 3>
template<eigen::concepts::ColVecs2Compatible VDerived, typename Container>
auto earclipping(const VDerived &V,
                 const Container &C) {
    return eigen::stl2eigen(earclipping_stl(V, C));
}

// ════════════════════════════════════════════════════════════════════════════
// Public API — Zipper (native, no as_eigen roundtrip)
// ════════════════════════════════════════════════════════════════════════════

template<zipper::concepts::ColVecs2Compatible VDerived, typename Container>
auto earclipping(const VDerived &V,
                 const Container &C) {
    auto r = earclipping_stl(V, std::span(C.begin(), C.end()));
    // StlMDArray gives (N,3) row-per-face; transpose to (3,N) col-per-face
    ::zipper::MatrixBase M = ::zipper::expression::nullary::get_const_non_owning_stl_storage(r);
    return M.transpose().eval();
}

template<zipper::concepts::ColVecs2Compatible VDerived, typename T>
auto earclipping(const VDerived &V,
                 const std::initializer_list<T> &C) {
    auto r = earclipping_stl(V, std::span(C));
    ::zipper::MatrixBase M = ::zipper::expression::nullary::get_const_non_owning_stl_storage(r);
    return M.transpose().eval();
}

template<zipper::concepts::ColVecs2Compatible VDerived, typename Index, zipper::index_type N>
auto earclipping(const VDerived &V,
                 const ::zipper::Vector<Index, N> &C) {
    auto r = earclipping_stl(V, C.expression().as_std_span());
    ::zipper::MatrixBase M = ::zipper::expression::nullary::get_const_non_owning_stl_storage(r);
    return M.transpose().eval();
}
}// namespace balsa::geometry::triangle_mesh
#endif
