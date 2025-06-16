#if !defined(BALSA_GEOMETRY_BOUNDINGBOX)
#define BALSA_GEOMETRY_BOUNDINGBOX
#include <zipper/types.hpp>
#include <zipper/Vector.hpp>
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
    BoundingBox();

    BoundingBox(zipper::index_type dim) :

                                          m_min(zipper::views::nullary::ConstantView<T>(zipper::create_dextents(dim), limits::max())),
                                          m_max(zipper::views::nullary::ConstantView<T>(zipper::create_dextents(dim), limits::min())) {}

    template<zipper::concepts::VectorBaseDerived Vec>
    void expand(const Vec &a);
    void expand(const BoundingBox &a);

    template<zipper::concepts::VectorBaseDerived Vec>
    bool contains(const Vec &a) const;
    bool contains(const BoundingBox &a) const;

    auto min() const { return m_min; }
    auto max() const { return m_max; }


  private:
    zipper::Vector<T, Dim> m_min = zipper::views::nullary::ConstantView<T>(limits::max());
    zipper::Vector<T, Dim> m_max = zipper::views::nullary::ConstantView<T>(limits::min());
};

template<typename T, zipper::rank_type Dim>
template<
  zipper::concepts::VectorBaseDerived MinVec,
  zipper::concepts::VectorBaseDerived MaxVec>
BoundingBox<T, Dim>::BoundingBox(const MinVec &m, const MaxVec &M) : m_min(m), m_max(M) {}

template<typename T, zipper::rank_type Dim>
template<
  zipper::concepts::VectorBaseDerived Vec>
BoundingBox<T, Dim>::BoundingBox(const Vec &m) : m_min(m), m_max(m) {
}
template<typename T, zipper::rank_type Dim>
BoundingBox<T, Dim>::BoundingBox() {}


template<typename T, zipper::rank_type Dim>
template<zipper::concepts::VectorBaseDerived Vec>
void BoundingBox<T, Dim>::expand(const Vec &a) {

    m_min = zipper::min(a.as_array(), m_min.as_array());
    m_max = zipper::max(a.as_array(), m_max.as_array());
}

template<typename T, zipper::rank_type Dim>
void BoundingBox<T, Dim>::expand(const BoundingBox &a) {

    m_min = zipper::min(a.m_min.as_array(), m_min.as_array());
    m_max = zipper::max(a.m_max.as_array(), m_max.as_array());
}
template<typename T, zipper::rank_type Dim>
template<zipper::concepts::VectorBaseDerived Vec>
bool BoundingBox<T, Dim>::contains(const Vec &a) const {

    (a.as_array() >= m_min.as_array()).all() && (a.as_array() <= m_max.as_array()).all();
}

template<typename T, zipper::rank_type Dim>
bool BoundingBox<T, Dim>::contains(const BoundingBox &a) const {

    (a.m_min.as_array() >= m_min.as_array()).all() && (a.m_max.as_array() <= m_max.as_array()).all();
}
}// namespace balsa::geometry
#endif
