#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <balsa/scene_graph/object.hpp>
#include <balsa/scene_graph/features/bounding_box.hpp>
#include <balsa/scene_graph/transformations/matrix_transformation.hpp>
#include <balsa/eigen/types.hpp>


TEST_CASE("object creation", "[object]") {
    using ETraits = balsa::scene_graph::embedding_traits<3, double>;
    using BBNode = balsa::scene_graph::features::BoundingBoxNode<ETraits>;
    using CBBNode = balsa::scene_graph::features::CachedBoundingBoxNode<ETraits>;
    using TType = balsa::scene_graph::transformations::MatrixTransformation<ETraits>;
    auto obj = std::make_shared<balsa::scene_graph::Object<TType>>();
    auto obj2 = std::make_shared<balsa::scene_graph::Object<TType>>();

    auto &obj_bb = obj->add_feature<BBNode>();

    obj->add_child(obj2);
    auto &obj_bb2 = obj->add_feature<CBBNode>();

    obj_bb2.set_bounding_box(Eigen::AlignedBox<double, 3>(balsa::eigen::Vec3d(-1, -2, -3), balsa::eigen::Vec3d(0, 0, 0)));

    auto obj3 = obj->emplace_child<decltype(obj)::element_type>();
    auto &obj_bb3 = obj->add_feature<CBBNode>();
    obj_bb3.set_bounding_box(Eigen::AlignedBox<double, 3>(balsa::eigen::Vec3d(1, 1, 1), balsa::eigen::Vec3d(1, 1, 1)));

    CHECK(obj2->parent() == obj3->parent());
    CHECK(obj == obj3->parent());

    CHECK(obj->children_size() == 2);
    CHECK(obj->children()[0] == obj2);
    CHECK(obj->children()[1] == obj3);

    obj_bb.add_child(&obj_bb2);
    obj_bb.add_child(&obj_bb3);
    auto bb = obj_bb.bounding_box();
    CHECK(bb.min().x() == -1);
    CHECK(bb.min().y() == -2);
    CHECK(bb.min().z() == -3);
    CHECK(bb.max().x() == 1);
    CHECK(bb.max().y() == 1);
    CHECK(bb.max().z() == 1);
}
