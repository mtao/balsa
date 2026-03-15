#if !defined(BALSA_EIGEN_ZIPPER_COMPAT_HPP)
#define BALSA_EIGEN_ZIPPER_COMPAT_HPP

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/MatrixBase.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/expression/nullary/NullaryExpressionBase.hpp>
#include <zipper/expression/detail/ExpressionTraits.hpp>
#include "balsa/eigen/concepts/matrix_types.hpp"


namespace balsa::eigen {
template<concepts::EigenBaseDerived EigenType, bool IsConst>
class EigenMatrixExpression;
template<concepts::EigenBaseDerived EigenType, bool IsConst>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
class EigenVectorExpression;
}// namespace balsa::eigen

namespace zipper::expression {

template<balsa::eigen::concepts::DenseBaseDerived EigenType, bool IsConst>
struct detail::ExpressionTraits<balsa::eigen::EigenMatrixExpression<EigenType, IsConst>>
  : public detail::BasicExpressionTraits<
      typename EigenType::Scalar,
      zipper::extents<zipper::index_type(EigenType::RowsAtCompileTime), zipper::index_type(EigenType::ColsAtCompileTime)>,
      detail::AccessFeatures{
        .is_const = IsConst,
        .is_reference = balsa::eigen::concepts::PlainObjectBaseDerived<EigenType> },
      detail::ShapeFeatures{ .is_resizable = false }> {
    constexpr static bool stores_references = true;
    constexpr static bool is_coefficient_consistent = true;
};

template<balsa::eigen::concepts::DenseBaseDerived EigenType, bool IsConst>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
struct detail::ExpressionTraits<balsa::eigen::EigenVectorExpression<EigenType, IsConst>>
  : public detail::BasicExpressionTraits<
      typename EigenType::Scalar,
      zipper::extents<zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>,
      detail::AccessFeatures{
        .is_const = IsConst,
        .is_reference = balsa::eigen::concepts::PlainObjectBaseDerived<EigenType> },
      detail::ShapeFeatures{ .is_resizable = false }> {
    constexpr static bool stores_references = true;
    constexpr static bool is_coefficient_consistent = true;
};

}// namespace zipper::expression

namespace balsa::eigen {
namespace detail {
    constexpr ::zipper::index_type get_index_type(Eigen::Index i) {
        if (i == Eigen::Dynamic) {
            return std::dynamic_extent;
        } else {
            return i;
        }
    }
}// namespace detail


template<concepts::EigenBaseDerived EigenType, bool IsConst>
class EigenMatrixExpression
  : public ::zipper::expression::nullary::NullaryExpressionBase<EigenMatrixExpression<EigenType, IsConst>>
  , public ::zipper::extents<detail::get_index_type(EigenType::RowsAtCompileTime), detail::get_index_type(EigenType::ColsAtCompileTime)> {

  public:
    using self_type = EigenMatrixExpression<EigenType, IsConst>;
    using extents_type = ::zipper::extents<detail::get_index_type(EigenType::RowsAtCompileTime), detail::get_index_type(EigenType::ColsAtCompileTime)>;
    using extents_traits = ::zipper::detail::ExtentsTraits<extents_type>;
    using traits_type = ::zipper::expression::detail::ExpressionTraits<self_type>;
    using value_type = EigenType::Scalar;

    using extents_type::extent;
    using extents_type::rank;
    auto extents() const -> const extents_type & { return *this; }

    EigenMatrixExpression(const EigenType &a)
        requires(extents_traits::rank_dynamic == 2)
      : extents_type(a.rows(), a.cols()), m_data(a) {}
    EigenMatrixExpression(const EigenType &a)
        requires(extents_traits::rank_dynamic == 1 && extents_traits::is_dynamic_extent(0))
      : extents_type(a.rows()), m_data(a) {}
    EigenMatrixExpression(const EigenType &a)
        requires(extents_traits::rank_dynamic == 1 && extents_traits::is_dynamic_extent(1))
      : extents_type(a.cols()), m_data(a) {}

    EigenMatrixExpression(const EigenType &a)
        requires(extents_traits::is_static)
      : m_data(a) {}

    value_type coeff(zipper::index_type a, zipper::index_type b) const {
        return m_data.coeff(a, b);
    }
    value_type &coeff_ref(zipper::index_type a, zipper::index_type b)
        requires(traits_type::is_writable)
    {
        return const_cast<EigenType &>(m_data).coeffRef(a, b);
    }
    const value_type &const_coeff_ref(zipper::index_type a, zipper::index_type b) const
        requires(traits_type::is_referrable())
    {
        return m_data.coeffRef(a, b);
    }

    auto make_owned() const { return this->eval(); }

  private:
    const EigenType &m_data;
};

template<concepts::EigenBaseDerived EigenType, bool IsConst>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
class EigenVectorExpression
  : public ::zipper::expression::nullary::NullaryExpressionBase<EigenVectorExpression<EigenType, IsConst>>
  , public ::zipper::extents<detail::get_index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)> {

  public:
    using self_type = EigenVectorExpression<EigenType, IsConst>;
    using extents_type = ::zipper::extents<detail::get_index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>;
    using extents_traits = ::zipper::detail::ExtentsTraits<extents_type>;
    using traits_type = ::zipper::expression::detail::ExpressionTraits<self_type>;
    using value_type = EigenType::Scalar;

    using extents_type::extent;
    using extents_type::rank;
    auto extents() const -> const extents_type & { return *this; }

    static_assert(extents_type::rank() == 1);

    EigenVectorExpression(const EigenType &a)
        requires(extents_traits::is_dynamic)
      : extents_type(a.rows() * a.cols()), m_data(a) {}
    EigenVectorExpression(const EigenType &a)
        requires(extents_traits::is_static)
      : m_data(a) {}

    value_type coeff(zipper::index_type a) const {
        return m_data.coeff(a);
    }
    value_type &coeff_ref(zipper::index_type a)
        requires(traits_type::is_writable)
    {
        return const_cast<EigenType &>(m_data).coeffRef(a);
    }
    const value_type &const_coeff_ref(zipper::index_type a) const
        requires(traits_type::is_referrable())
    {
        return m_data.coeffRef(a);
    }

    auto begin() { return m_data.begin(); }
    auto end() { return m_data.end(); }
    auto begin() const { return m_data.begin(); }
    auto end() const { return m_data.end(); }

    auto make_owned() const { return this->eval(); }

  private:
    const EigenType &m_data;
};

template<bool IsConst, concepts::EigenBaseDerived EigenType>
auto as_zipper_2d(EigenType const &t) {
    if constexpr (concepts::MatrixBaseDerived<EigenType>) {
        return ::zipper::MatrixBase<EigenMatrixExpression<EigenType, IsConst>>(EigenMatrixExpression<EigenType, IsConst>(t));
    } else if constexpr (concepts::ArrayBaseDerived<EigenType>) {
        return ::zipper::ArrayBase<EigenMatrixExpression<EigenType, IsConst>>(EigenMatrixExpression<EigenType, IsConst>(t));
    }
}
template<bool IsConst, concepts::EigenBaseDerived EigenType>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
auto as_zipper_1d(EigenType const &t) {
    if constexpr (concepts::MatrixBaseDerived<EigenType>) {
        return ::zipper::VectorBase<EigenVectorExpression<EigenType, IsConst>>(EigenVectorExpression<EigenType, IsConst>(t));
    } else if constexpr (concepts::ArrayBaseDerived<EigenType>) {
        return ::zipper::ArrayBase<EigenVectorExpression<EigenType, IsConst>>(EigenVectorExpression<EigenType, IsConst>(t));
    }
}
template<concepts::EigenBaseDerived EigenType>
auto as_zipper(EigenType &t) {

    if constexpr (EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1) {
        return as_zipper_1d<false>(t);
    } else {
        return as_zipper_2d<false>(t);
    }
}
template<concepts::EigenBaseDerived EigenType>
auto as_zipper(EigenType const &t) {

    if constexpr (EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1) {
        return as_zipper_1d<true>(t);
    } else {
        return as_zipper_2d<true>(t);
    }
}

namespace detail {
    constexpr inline Eigen::Index zipper_to_eigen_dim(const zipper::index_type &index) {
        if (index == std::dynamic_extent) {
            return Eigen::Dynamic;
        } else {
            return Eigen::Index(index);
        }
    }
    constexpr inline zipper::index_type eigen_to_zipper_dim(const Eigen::Index &index) {
        if (index == Eigen::Dynamic) {
            return std::dynamic_extent;
        } else {
            return zipper::index_type(index);
        }
    }
}// namespace detail

template<::zipper::concepts::Matrix ZipType>
auto as_eigen(const ZipType &zip) {
    using extents_type = ZipType::extents_type;
    using MatType = Eigen::Matrix<typename ZipType::value_type,
                                  detail::zipper_to_eigen_dim(extents_type::static_extent(0)),
                                  detail::zipper_to_eigen_dim(extents_type::static_extent(1))>;
    return MatType::NullaryExpr(zip.extent(0), zip.extent(1), [&zip](Eigen::Index a, Eigen::Index b) {
        return zip(a, b);
    });
}
template<::zipper::concepts::Vector ZipType>
auto as_eigen(const ZipType &zip) {

    using extents_type = ZipType::extents_type;
    using MatType = Eigen::Matrix<typename ZipType::value_type,
                                  detail::zipper_to_eigen_dim(extents_type::static_extent(0)),
                                  1>;
    return MatType::NullaryExpr(zip.extent(0), [&zip](Eigen::Index a) {
        return zip(a);
    });
}


}// namespace balsa::eigen

#endif
