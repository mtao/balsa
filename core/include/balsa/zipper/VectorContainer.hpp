#if !defined(BALSA_ZIPPER_VECTOR_CONTAINER_HPP)
#define BALSA_ZIPPER_VECTOR_CONTAINER_HPP

#include <zipper/ZipperBase.hpp>
#include <zipper/concepts/ViewDerived.hpp>

namespace balsa::zipper {

template<typename ValueType, index_type Rows, index_type Cols, bool RowMajor>
class VectorContainer : public VectorContainerBase<storage::PlainObjectStorage<ValueType, zipper::extents<Rows, Cols>, std::conditional_t<RowMajor, std::experimental::layout_left, std::experimental::layout_right>>> {
  public:
    using layout_type =
      std::conditional_t<RowMajor, std::experimental::layout_left, std::experimental::layout_right>;
    using Base = VectorContainerBase<storage::PlainObjectStorage<
      ValueType,
      zipper::extents<Rows, Cols>,
      layout_type

      >>;
    using Base::view;
    using view_type = Base::view_type;
    using value_type = Base::value_type;
    using extents_type = Base::extents_type;
    using Base::col;
    using Base::extent;
    using Base::extents;
    using Base::row;
    using extents_traits = detail::ExtentsTraits<extents_type>;
    using span_type = VectorContainerSpan<ValueType, Rows, Cols, RowMajor>;
    using Base::transpose;

    VectorContainer()
        requires(extents_traits::is_static)
    = default;

    VectorContainer(index_type dyn_size)
        requires(extents_traits::rank_dynamic == 1)
      : Base(extents_type(dyn_size)) {}

    VectorContainer(index_type rows, index_type cols)
        requires(extents_traits::is_dynamic)
      : Base(extents_type(rows, cols)) {}

    VectorContainer(const extents_type &e)
        requires(extents_traits::is_dynamic)
      : Base(e) {}
    VectorContainer(const extents_type &)
        requires(extents_traits::is_static)
      : Base() {}

    VectorContainer(index_type rows, index_type cols)
        requires(extents_traits::is_static)
      : Base() {}

    template<concepts::VectorContainerBaseDerived Other>
    VectorContainer(const Other &other) : Base(other) {}

    template<concepts::ViewDerived Other>
    VectorContainer(const Other &other) : Base(other) {}

    VectorContainer(const VectorContainer &other) : Base(other.view()) {}
    template<index_type R2, index_type C2>
    VectorContainer(const VectorContainer<value_type, R2, C2> &other) : Base(other.view()) {}
    // template <concepts::VectorContainerViewDerived Other>
    // VectorContainer(const Other& other) : Base(other) {}
    // template <concepts::VectorContainerBaseDerived Other>
    // VectorContainer(const Other& other) : Base(other) {}
    // template <concepts::ViewDerived Other>
    // VectorContainer(const Other& other) : Base(other) {}
    // template <typename... Args>
    // VectorContainer(Args&&... args)
    //     requires(concepts::IndexPackLike<Args...>)
    //     : Base(zipper::extents<Rows, Cols>(std::forward<Args>(args)...)) {}
    // template <index_type... indices>
    // VectorContainer(const zipper::extents<indices...>& e) : Base(e) {}
    using Base::operator=;
    VectorContainer &operator=(VectorContainer &&other) {
        Base::operator=(std::move(other.view()));
        return *this;
    }

    VectorContainer &operator=(const VectorContainer &other) {
        Base::operator=(other.view());
        return *this;
    }
    template<index_type R2, index_type C2>
    VectorContainer &operator=(const VectorContainer<value_type, R2, C2> &other) {
        Base::operator=(other.view());
        return *this;
    }

    template<typename... Args>
    auto operator()(Args &&...idxs) const -> decltype(auto)

    {
        decltype(auto) r = view()(std::forward<Args>(idxs)...);
        if constexpr (std::is_same_v<std::decay_t<decltype(r)>, value_type>) {
            return r;
        } else {
            using R = typename std::decay_t<decltype(r)>;
            if constexpr (R::extents_type::rank() == 1) {
                return VectorBase<R>(r);
            } else if constexpr (R::extents_type::rank() == 2) {
                return VectorContainerBase<R>(r);
            }
        }
    }
    template<typename... Args>
    auto operator()(Args &&...idxs) -> decltype(auto)

    {
        decltype(auto) r = view()(std::forward<Args>(idxs)...);
        if constexpr (std::is_same_v<std::decay_t<decltype(r)>, value_type>) {
            return r;
        } else {
            using R = typename std::decay_t<decltype(r)>;
            if constexpr (R::extents_type::rank() == 1) {
                return VectorBase<R>(r);
            } else if constexpr (R::extents_type::rank() == 2) {
                return VectorContainerBase<R>(r);
            }
        }
    }
};
template<concepts::VectorContainerViewDerived MB>

}// namespace balsa::zipper
#endif

