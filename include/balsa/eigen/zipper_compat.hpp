#if !defined(BALSA_EIGEN_ZIPPER_COMPAT_HPP)
#define BALSA_EIGEN_ZIPPER_COMPAT_HPP

#include <zipper/MatrixBase.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/views/nullary/NullaryViewBase.hpp>
#include "balsa/eigen/concepts/matrix_types.hpp"


namespace balsa::eigen {
template<concepts::EigenBaseDerived EigenType>
class EigenMatrixView;
template<concepts::EigenBaseDerived EigenType>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
class EigenVectorView;
}// namespace balsa::eigen
namespace zipper::views::detail {

template<balsa::eigen::concepts::DenseBaseDerived EigenType>
struct ViewTraits<balsa::eigen::EigenMatrixView<EigenType>>
  : public views::detail::DefaultViewTraits<
      typename EigenType::Scalar,
      zipper::extents<zipper::index_type(EigenType::RowsAtCompileTime), zipper::index_type(EigenType::ColsAtCompileTime)>> {
    // to pass a base type to the UnaryViewBase
    template<typename Derived>
    constexpr static bool holds_extents = false;
    constexpr static bool is_value_based = balsa::eigen::concepts::PlainObjectBaseDerived<EigenType>;
    constexpr static bool is_const = false;

    template<typename Derived>
    using base_type = DimensionedViewBase<Derived>;
};

template<balsa::eigen::concepts::DenseBaseDerived EigenType>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
struct ViewTraits<balsa::eigen::EigenVectorView<EigenType>>
  : public views::detail::DefaultViewTraits<
      typename EigenType::Scalar,
      zipper::extents<zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>> {
    // to pass a base type to the UnaryViewBase
    template<typename Derived>
    constexpr static bool holds_extents = false;
    constexpr static bool is_value_based = balsa::eigen::concepts::PlainObjectBaseDerived<EigenType>;
    constexpr static bool is_const = false;
    template<typename Derived>
    using base_type = DimensionedViewBase<Derived>;
};
}// namespace zipper::views::detail
namespace balsa::eigen {


template<concepts::EigenBaseDerived EigenType>
class EigenMatrixView : public zipper::views::nullary::NullaryViewBase<EigenMatrixView<EigenType>, typename EigenType::Scalar, EigenType::RowsAtCompileTime, EigenType::ColsAtCompileTime> {

  public:
    using Base = zipper::views::nullary::NullaryViewBase<EigenMatrixView<EigenType>, typename EigenType::Scalar, zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>;
    using traits_type = zipper::views::detail::ViewTraits<EigenMatrixView<EigenType>>;
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
    value_type &coeff_ref(zipper::index_type a, zipper::index_type b) const
        requires traits_type::is_value_based
    {
        return m_data.coeffRef(a, b);
    }
    const value_type &const_coeff_ref(zipper::index_type a, zipper::index_type b) const
        requires traits_type::is_value_based
    {
        return m_data.coeffRef(a, b);
    }

  private:
    const EigenType &m_data;
};

template<concepts::EigenBaseDerived EigenType>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
class EigenVectorView : public zipper::views::nullary::NullaryViewBase<EigenVectorView<EigenType>, typename EigenType::Scalar, zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)> {

  public:
    using Base = zipper::views::nullary::NullaryViewBase<EigenVectorView<EigenType>, typename EigenType::Scalar, zipper::index_type(EigenType::RowsAtCompileTime *EigenType::ColsAtCompileTime)>;
    static_assert(Base::extents_type::rank() == 1);
    using traits_type = zipper::views::detail::ViewTraits<EigenVectorView<EigenType>>;
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
    value_type &coeff_ref(zipper::index_type a, zipper::index_type b) const
        requires traits_type::is_value_based
    {
        return m_data.coeffRef(a, b);
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

template<concepts::EigenBaseDerived EigenType>
auto as_zipper_2d(EigenType const &t) {
    if constexpr (concepts::MatrixBaseDerived<EigenType>) {
        return zipper::MatrixBase(EigenMatrixView(t));
    } else if constexpr (concepts::ArrayBaseDerived<EigenType>) {
        return zipper::ArrayBase(EigenMatrixView(t));
    }
}
template<bool IsConst, concepts::EigenBaseDerived EigenType>
    requires(EigenType::RowsAtCompileTime == 1 || EigenType::ColsAtCompileTime == 1)
auto as_zipper_1d(EigenType const &t) {
    if constexpr (concepts::MatrixBaseDerived<EigenType>) {
        return zipper::VectorBase<EigenVectorView<EigenType>>(EigenVectorView<EigenType>(t));
    } else if constexpr (concepts::ArrayBaseDerived<EigenType>) {
        return zipper::ArrayBase<EigenVectorView<EigenType>>(EigenVectorView<EigenType>(t));
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
}// namespace balsa::eigen

#endif
