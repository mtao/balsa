#if !defined(BALSA_GEOMETRY_BOUNDINGBOX)
#define BALSA_GEOMETRY_BOUNDINGBOX
#include <zipper/types.hpp>
#include <zipper/Vector.hpp>
#include <bitset>
#include <zipper/as.hpp>
#include <zipper/views/nullary/ConstantView.hpp>
#include <zipper/concepts/VectorBaseDerived.hpp>
#include <limits>


namespace balsa::geometry {
template<typename T, zipper::rank_type Dim = std::dynamic_extent>
class BoundingBox {
  private:
    using limits = std::numeric_limits<T>;

  public:
    template<
      zipper::concepts::VectorBaseDerived MinVec,
      zipper::concepts::VectorBaseDerived MaxVec>
    BoundingBox(const MinVec &m, const MaxVec &M);

    template<
      zipper::concepts::VectorBaseDerived Vec>
    BoundingBox(const Vec &m);
    BoundingBox() = default;

    BoundingBox(BoundingBox &&) = default;
    BoundingBox(const BoundingBox &) = default;
    BoundingBox &operator=(BoundingBox &&) = default;
    BoundingBox &operator=(const BoundingBox &) = default;

    BoundingBox(zipper::index_type dim) :

                                          m_min(zipper::views::nullary::ConstantView<T>(zipper::create_dextents(dim), limits::max())),
                                          m_max(zipper::views::nullary::ConstantView<T>(zipper::create_dextents(dim), limits::lowest())) {}

    template<zipper::concepts::VectorBaseDerived Vec>
    void expand(const Vec &a);
    void expand(const BoundingBox &a);

    template<zipper::concepts::VectorBaseDerived Vec>
    bool contains(const Vec &a) const;
    bool contains(const BoundingBox &a) const;

    auto range() const {
        return m_max - m_min;
    }

    zipper::Vector<T, Dim> corner(const std::bitset<Dim> &c) const;

    const auto &min() const { return m_min; }
    const auto &max() const { return m_max; }

  private:
    zipper::Vector<T, Dim> m_min = zipper::views::nullary::ConstantView<T>(limits::max());
    zipper::Vector<T, Dim> m_max = zipper::views::nullary::ConstantView<T>(limits::lowest());
};


// make sure the types are teh same and the extents are valid
template<
  zipper::concepts::VectorBaseDerived MinVec,
  zipper::concepts::VectorBaseDerived MaxVec>
    requires(std::is_same_v<typename MinVec::value_type, typename MaxVec::value_type>
             && (MinVec::extents_type::static_extent(0) == std::dynamic_extent
                 || MinVec::extents_type::static_extent(0) == std::dynamic_extent || MaxVec::extents_type::static_extent(0) == MinVec::extents_type::static_extent(0)))
BoundingBox(const MinVec &m, const MaxVec &M) -> BoundingBox<typename MinVec::value_type,
                                                             MinVec::extents_type::static_extent(0) == std::dynamic_extent ? MaxVec::extents_type::static_extent(0) : MinVec::extents_type::static_extent(0)>;


template<
  zipper::concepts::VectorBaseDerived Vec>
BoundingBox(const Vec &) -> BoundingBox<typename Vec::value_type,
                                        Vec::extents_type::static_extent(0)>;


template<typename T, zipper::rank_type Dim>
template<
  zipper::concepts::VectorBaseDerived MinVec,
  zipper::concepts::VectorBaseDerived MaxVec>
BoundingBox<T, Dim>::BoundingBox(const MinVec &m, const MaxVec &M) : m_min(m), m_max(M) {

    constexpr bool min_ext = MinVec::extents_type::static_extent(0) == std::dynamic_extent;
    constexpr bool max_ext = MaxVec::extents_type::static_extent(0) == std::dynamic_extent;
    if constexpr (min_ext && max_ext) {
        assert(m.extent(0) == M.extent(0));
    } else if constexpr (min_ext) {
        assert(m.extent(0) == Dim);
    } else if constexpr (max_ext) {
        assert(M.extent(0) == Dim);
    }
}

template<typename T, zipper::rank_type Dim>
template<
  zipper::concepts::VectorBaseDerived Vec>
BoundingBox<T, Dim>::BoundingBox(const Vec &m) : m_min(m), m_max(m) {
}

template<typename T, zipper::rank_type Dim>

auto BoundingBox<T, Dim>::corner(const std::bitset<Dim> &c) const -> zipper::Vector<T, Dim> {
    zipper::Vector<T, Dim> D;
    for (zipper::rank_type j = 0; j < Dim; ++j) {
        D(j) = c[j] ? max()(j) : min()(j);
    }
    return D;
}

template<typename T, zipper::rank_type Dim>
template<zipper::concepts::VectorBaseDerived Vec>
void BoundingBox<T, Dim>::expand(const Vec &a) {

    m_min = zipper::as_vector(zipper::min(a.as_array(), m_min.as_array()));
    m_max = zipper::as_vector(zipper::max(a.as_array(), m_max.as_array()));
}

template<typename T, zipper::rank_type Dim>
void BoundingBox<T, Dim>::expand(const BoundingBox &a) {

    m_min = zipper::as_vector(zipper::min(a.m_min.as_array(), m_min.as_array()));
    m_max = zipper::as_vector(zipper::max(a.m_max.as_array(), m_max.as_array()));
}
template<typename T, zipper::rank_type Dim>
template<zipper::concepts::VectorBaseDerived Vec>
bool BoundingBox<T, Dim>::contains(const Vec &x) const {

    auto xa = x.array();
    return ((m_min.array() <= xa) && (m_max.array() >= xa)).all();
}

template<typename T, zipper::rank_type Dim>
bool BoundingBox<T, Dim>::contains(const BoundingBox &o) const {

    ((m_min.array() <= o.m_min.array()) && (m_max.array() >= o.m_max.array())).all();
}
}// namespace balsa::geometry
#endif
