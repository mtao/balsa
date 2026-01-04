#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_CAMERA_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_CAMERA_HPP

#include <memory>
#include "object.hpp"

namespace balsa::scene_graph {
namespace objects {
    class Node;
}


template<concepts::abstract_transformation TransformationType>
class Camera : public Object<TransformationType> {
  public:
  private:
};

}// namespace balsa::scene_graph

#endif
