#if !defined(BALSA_EIGEN_ZIPPER_COMPAT_HPP)
#define BALSA_EIGEN_ZIPPER_COMPAT_HPP

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/MatrixBase.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/views/nullary/NullaryViewBase.hpp>
#include "balsa/eigen/concepts/matrix_types.hpp"


namespace balsa::eigen {
template<concepts::EigenBaseDerived EigenType, bool IsConst>
class EigenMatrixView;
template<concepts::EigenBaseDerived EigenType, bool IsConst>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
class EigenVectorView;
}// namespace balsa::eigen
namespace zipper::views::detail {

template<balsa::eigen::concepts::DenseBaseDerived EigenType, bool IsConst>
struct ViewTraits<balsa::eigen::EigenMatrixView<EigenType, IsConst>>
  : public views::detail::DefaultViewTraits<
      typename EigenType::Scalar,
      zipper::extents<zipper::index_type(EigenType::RowsAtCompileTime), zipper::index_type(EigenType::ColsAtCompileTime)>> {
    // to pass a base type to the UnaryViewBase
    template<typename Derived>
    constexpr static bool holds_extents = false;
    constexpr static bool is_value_based = balsa::eigen::concepts::PlainObjectBaseDerived<EigenType>;
    constexpr static bool is_const = IsConst;

    template<typename Derived>
    using base_type = DimensionedViewBase<Derived>;
};

template<balsa::eigen::concepts::DenseBaseDerived EigenType, bool IsConst>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
struct ViewTraits<balsa::eigen::EigenVectorView<EigenType, IsConst>>
  : public views::detail::DefaultViewTraits<
      typename EigenType::Scalar,
      zipper::extents<zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>> {
    // to pass a base type to the UnaryViewBase
    template<typename Derived>
    constexpr static bool holds_extents = false;
    constexpr static bool is_value_based = balsa::eigen::concepts::PlainObjectBaseDerived<EigenType>;
    constexpr static bool is_const = IsConst;
    template<typename Derived>
    using base_type = DimensionedViewBase<Derived>;
};
}// namespace zipper::views::detail
namespace balsa::eigen {


template<concepts::EigenBaseDerived EigenType, bool IsConst>
class EigenMatrixView : public zipper::views::nullary::NullaryViewBase<EigenMatrixView<EigenType, IsConst>, typename EigenType::Scalar, EigenType::RowsAtCompileTime, EigenType::ColsAtCompileTime> {

  public:
    using Base = zipper::views::nullary::NullaryViewBase<EigenMatrixView<EigenType, IsConst>, typename EigenType::Scalar, zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>;
    using traits_type = zipper::views::detail::ViewTraits<EigenMatrixView<EigenType, IsConst>>;
    using value_type = EigenType::Scalar;
    using extents_type = zipper::extents<EigenType::RowsAtCompileTime, EigenType::ColsAtCompileTime>;
    using extents_traits = zipper::detail::ExtentsTraits<extents_type>;

    EigenMatrixView(const EigenType &a)
        requires(extents_traits::is_dynamic)
      : Base(extents_type(a.rows() * a.cols())), m_data(a) {}
    EigenMatrixView(const EigenType &a)
        requires(extents_traits::is_static)
      : m_data(a) {}

    value_type coeff(zipper::index_type a, zipper::index_type b) const {
        return m_data.coeff(a, b);
    }
    value_type &coeff_ref(zipper::index_type a, zipper::index_type b)
        requires(traits_type::is_value_based && traits_type::is_const)
    {
        return const_cast<EigenType &>(m_data).coeffRef(a, b);
    }
    const value_type &const_coeff_ref(zipper::index_type a, zipper::index_type b) const
        requires traits_type::is_value_based
    {
        return m_data.coeffRef(a, b);
    }

  private:
    const EigenType &m_data;
};

template<concepts::EigenBaseDerived EigenType, bool IsConst>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
class EigenVectorView : public zipper::views::nullary::NullaryViewBase<EigenVectorView<EigenType, IsConst>, typename EigenType::Scalar, zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)> {

  public:
    using Base = zipper::views::nullary::NullaryViewBase<EigenVectorView<EigenType, IsConst>, typename EigenType::Scalar, zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>;
    static_assert(Base::extents_type::rank() == 1);
    using traits_type = zipper::views::detail::ViewTraits<EigenVectorView<EigenType, IsConst>>;
    using value_type = EigenType::Scalar;
    using extents_type = zipper::extents<zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>;
    using extents_traits = zipper::detail::ExtentsTraits<extents_type>;

    EigenVectorView(const EigenType &a)
        requires(extents_traits::is_dynamic)
      : Base(extents_type(a.rows() * a.cols())), m_data(a) {}
    EigenVectorView(const EigenType &a)
        requires(extents_traits::is_static)
      : m_data(a) {}

    value_type coeff(zipper::index_type a, zipper::index_type b) const {
        return m_data.coeff(a, b);
    }
    value_type &coeff_ref(zipper::index_type a, zipper::index_type b)
        requires(traits_type::is_value_based && traits_type::is_const)
    {
        return const_cast<EigenType &>(m_data).coeffRef(a, b);
    }
    const value_type &const_coeff_ref(zipper::index_type a, zipper::index_type b) const
        requires traits_type::is_value_based
    {
        return m_data.coeffRef(a, b);
    }

    auto begin() { return m_data.begin(); }
    auto end() { return m_data.end(); }
    auto begin() const { return m_data.begin(); }
    auto end() const { return m_data.end(); }

  private:
    const EigenType &m_data;
};

template<bool IsConst, concepts::EigenBaseDerived EigenType>
auto as_zipper_2d(EigenType const &t) {
    if constexpr (concepts::MatrixBaseDerived<EigenType>) {
        return zipper::MatrixBase<EigenMatrixView<EigenType, IsConst>>(EigenMatrixView<EigenType, IsConst>(t));
    } else if constexpr (concepts::ArrayBaseDerived<EigenType>) {
        return zipper::ArrayBase<EigenMatrixView<EigenType, IsConst>>(EigenMatrixView<EigenType, IsConst>(t));
    }
}
template<bool IsConst, concepts::EigenBaseDerived EigenType>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
auto as_zipper_1d(EigenType const &t) {
    if constexpr (concepts::MatrixBaseDerived<EigenType>) {
        return zipper::VectorBase<EigenVectorView<EigenType, IsConst>>(EigenVectorView<EigenType, IsConst>(t));
    } else if constexpr (concepts::ArrayBaseDerived<EigenType>) {
        return zipper::ArrayBase<EigenVectorView<EigenType, IsConst>>(EigenVectorView<EigenType, IsConst>(t));
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

template<zipper::concepts::MatrixBaseDerived ZipType>
auto as_eigen(const ZipType &zip) {
    using extents_type = ZipType::extents_type;
    using MatType = Eigen::Matrix<typename ZipType::value_type,
                                  detail::zipper_to_eigen_dim(extents_type::static_extent(0)),
                                  detail::zipper_to_eigen_dim(extents_type::static_extent(1))>;
    return MatType::NullaryExpr(zip.extent(0), zip.extent(1), [&zip](Eigen::Index a, Eigen::Index b) {
        return zip(a, b);
    });
}
template<zipper::concepts::VectorBaseDerived ZipType>
auto as_eigen(const ZipType &zip) {

    using extents_type = ZipType::extents_type;
    using MatType = Eigen::Matrix<typename ZipType::value_type,
                                  detail::zipper_to_eigen_dim(extents_type::static_extent(0)),
                                  1>;
    return MatType::NullaryExpr(zip.extent(0), zip.extent(1), [&zip](Eigen::Index a) {
        return zip(a);
    });
}


}// namespace balsa::eigen

#endif
