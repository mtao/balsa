#if !defined(BALSA_GEOMETRY_ACCELERATION_ALTERNATING_DIGITAL_TREE_HPP)
#define BALSA_GEOMETRY_ACCELERATION_ALTERNATING_DIGITAL_TREE_HPP
#include "balsa/geometry/BoundingBox.hpp"
#include <memory>


namespace balsa::geometry::acceleration {

#if BALSA_HAS_QUIVER

template<int D>
class AlternatingDigitalTree {
  public:
    using Scalar = double;
    constexpr static int Dim = D;
    using BoundingBox = geometry::BoundingBox<D>;


    // void insert(AlignedBox);

  private:
    template<int ND>
    struct Node {
        constexpr static int CD = (ND + 1) % Dim;
        using ChildNodeType = Node<CD>;
        using ChildNodeHandleType = std::unique_ptr<ChildNodeType>;
        struct InternalNode {
            ChildNodeHandleType left = nullptr;
            ChildNodeHandleType right = nullptr;


            template<zipper::concepts::Vector Vec>
            std::weak_ordering operator<=>(const Vec &v) const {
                return dividing_position <=> v(CD);
            }
            double dividing_position;
        };

        struct LeafNode {
            BoundingBox _bounding_box;
        };
    };

    Node<0> _root = nullptr;
};

#else// !BALSA_HAS_QUIVER

template<typename T, int D>
class AlternatingDigitalTree {
  public:
    using Scalar = T;
    constexpr static int Dim = D;
    using BoundingBox = geometry::BoundingBox<T, D>;


    // void insert(AlignedBox);

  private:
    template<int ND>
    struct Node {
        constexpr static int CD = (ND + 1) % Dim;
        using ChildNodeType = Node<CD>;
        using ChildNodeHandleType = std::unique_ptr<ChildNodeType>;
        struct InternalNode {
            ChildNodeHandleType left = nullptr;
            ChildNodeHandleType right = nullptr;


            template<zipper::concepts::Vector Vec>
            std::weak_ordering operator<=>(const Vec &v) const {
                return dividing_position <=> v(CD);
            }
            Scalar dividing_position;
        };

        struct LeafNode {
            BoundingBox _bounding_box;
        };
    };

    Node<0> _root = nullptr;
};

#endif// BALSA_HAS_QUIVER

}// namespace balsa::geometry::acceleration
#endif
