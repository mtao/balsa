
#if !defined(BALSA_ZIPPER_VECTOR_CONTAINER_BASE_HPP)
#define BALSA_ZIPPER_VECTOR_CONTAINER_BASE_HPP

#include <zipper/ZipperBase.hpp>
#include <zipper/concepts/ViewDerived.hpp>

namespace balsa::zipper {


// template <concepts::VectorContainerViewDerived View>
template<zipper::concepts::ViewDerived View>
class VectorContainerBase : public zipper::ZipperBase<VectorContainerBase, View> {
  public:
    VectorContainerBase() = default;

    using view_type = View;
    using value_type = View::value_type;
    using extents_type = View::extents_type;
    using extents_traits = detail::ExtentsTraits<extents_type>;
    static_assert(extents_traits::rank == 2);
    using Base = ZipperBase<VectorContainerBase, View>;

    using Base::Base;
    // using Base::operator=;
    using Base::cast;
    using Base::swizzle;
    using Base::view;

    auto eval() const { return VectorContainer(*this); }

    template<concepts::VectorContainerBaseDerived Other>
    VectorContainerBase(const Other &other)
        requires(view_type::is_writable)
      : VectorContainerBase(other.view()) {}

    VectorContainerBase &operator=(concepts::VectorContainerBaseDerived auto const &v) {
        return operator=(v.view());
    }

    template<concepts::VectorContainerViewDerived Other>
    VectorContainerBase &operator=(const Other &other)
        requires(view_type::is_writable)
    {
        return Base::operator=(other);
    }

    auto as_array() const {
        return ArrayBase<views::unary::IdentityView<View>>(view());
    }

    value_type trace() const { return views::reductions::Trace(view())(); }

  public:
    template<typename Slice>
    auto row() {
        return slice<Slice, full_extent_t>();
    }
    template<typename Slice>
    auto col() {
        return slice<full_extent_t, Slice>();
    }

    template<typename Slice>
    auto row() const {
        return slice<Slice, full_extent_t>();
    }
    template<typename Slice>
    auto col() const {
        return slice<full_extent_t, Slice>();
    }

    template<typename Slice>
    auto row(Slice &&s) {
        return slice<Slice, full_extent_t>(std::forward<Slice>(s),
                                           full_extent_t{});
    }
    template<typename Slice>
    auto col(Slice &&s) {
        return slice<full_extent_t, Slice>(full_extent_t{},
                                           std::forward<Slice>(s));
    }

    template<typename Slice>
    auto row(Slice &&s) const {
        return slice<Slice, full_extent_t>(std::forward<Slice>(s),
                                           full_extent_t{});
    }
    template<typename Slice>
    auto col(Slice &&s) const {
        return slice<full_extent_t, Slice>(full_extent_t{}, std::forward<Slice>(s));
    }
    template<typename... Slices>
    auto slice() {
        auto v = Base::template slice_view<Slices...>();
        using V = std::decay_t<decltype(v)>;
        if constexpr (V::extents_type::rank() == 2) {
            return VectorContainerBase<V>(std::move(v));
        } else {
            static_assert(V::extents_type::rank() == 1);
            return VectorBase<V>(std::move(v));
        }
    }

    template<typename... Slices>
    auto slice(Slices &&...slices) const {
        auto v = Base::template slice_view<Slices...>(
          std::forward<Slices>(slices)...);
        using V = std::decay_t<decltype(v)>;
        if constexpr (V::extents_type::rank() == 2) {
            return VectorContainerBase<V>(std::move(v));
        } else {
            static_assert(V::extents_type::rank() == 1);
            return VectorBase<V>(std::move(v));
        }
    }
    template<typename... Slices>
    auto slice() const {
        auto v = Base::template slice_view<Slices...>();
        using V = std::decay_t<decltype(v)>;
        if constexpr (V::extents_type::rank() == 2) {
            return VectorContainerBase<V>(std::move(v));
        } else {
            static_assert(V::extents_type::rank() == 1);
            return VectorBase<V>(std::move(v));
        }
    }

    template<typename... Slices>
    auto slice(Slices &&...slices) {
        auto v = Base::template slice_view<Slices...>(
          std::forward<Slices>(slices)...);
        using V = std::decay_t<decltype(v)>;
        if constexpr (V::extents_type::rank() == 2) {
            return VectorContainerBase<V>(std::move(v));
        } else {
            static_assert(V::extents_type::rank() == 1);
            return VectorBase<V>(std::move(v));
        }
    }

    auto diagonal() const {
        return VectorBase<views::unary::DiagonalView<view_type, true>>(view());
    }
    auto diagonal() {
        return VectorBase<views::unary::DiagonalView<view_type, false>>(view());
    }
    auto transpose() const { return swizzle<VectorContainerBase, 1, 0>(); }

    template<rank_type... ranks>
    auto swizzle() const {
        return Base::template swizzle<VectorContainerBase, ranks...>();
    }
};

template<concepts::VectorContainerViewDerived View>
VectorContainerBase(View &&view) -> VectorContainerBase<View>;
template<concepts::VectorContainerViewDerived View>
VectorContainerBase(const View &view) -> VectorContainerBase<View>;

UNARY_DECLARATION(VectorContainerBase, LogicalNot, operator!)
UNARY_DECLARATION(VectorContainerBase, BitNot, operator~)
UNARY_DECLARATION(VectorContainerBase, Negate, operator-)

SCALAR_BINARY_DECLARATION(VectorContainerBase, Divides, operator/)

BINARY_DECLARATION(VectorContainerBase, Plus, operator+)
BINARY_DECLARATION(VectorContainerBase, Minus, operator-)
//

template<concepts::VectorContainerBaseDerived View1, concepts::VectorContainerBaseDerived View2>
auto operator==(View1 const &lhs, View2 const &rhs) {
    return (lhs.as_array() == rhs.as_array()).all();
}
template<concepts::VectorContainerBaseDerived View1, concepts::VectorContainerBaseDerived View2>
auto operator!=(View1 const &lhs, View2 const &rhs) {
    return (lhs.as_array() == rhs.as_array()).all();
}

template<concepts::VectorContainerBaseDerived View1, concepts::VectorContainerBaseDerived View2>
auto operator*(View1 const &lhs, View2 const &rhs) {
    using V = views::binary::VectorContainerProductView<typename View1::view_type,
                                                        typename View2::view_type>;
    return VectorContainerBase<V>(V(lhs.view(), rhs.view()));
}

template<concepts::VectorContainerBaseDerived View>
auto operator*(View const &lhs, typename View::value_type const &rhs) {
    using V =
      views::unary::ScalarMultipliesView<typename View::value_type,
                                         typename View::view_type,
                                         true>;
    return VectorContainerBase<V>(V(lhs.view(), rhs));
}
template<concepts::VectorContainerBaseDerived View>
auto operator*(typename View::value_type const &lhs, View const &rhs) {
    using V =
      views::unary::ScalarMultipliesView<typename View::value_type,
                                         typename View::view_type,
                                         false>;
    return VectorContainerBase<V>(V(lhs, rhs.view()));
}

template<concepts::VectorContainerBaseDerived View1, concepts::VectorBaseDerived View2>
auto operator*(View1 const &lhs, View2 const &rhs) {
    using V = views::binary::VectorContainerVectorProductView<typename View1::view_type,
                                                              typename View2::view_type>;

    return VectorBase<V>(V(lhs.view(), rhs.view()));
}


}// namespace balsa::zipper
#endif

