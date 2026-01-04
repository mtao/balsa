#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP

#include <vector>
#include "abstract_feature.hpp"
#include "embedding_traits.hpp"

namespace balsa::visualization::scene_graph{


    template <concepts::embedding_traits embedding_traits>
    class AbstractFeatureGroup {
        public:
        using scalar_type = Scalar;
        constexpr static int dimension = D;
        using abstract_feature_type = AbstractFeature<D,Scalar>;

        private:
        template<int, typename,typename> friend class FeatureGroup;
        void add(abstract_feature_type& feature);
        void remove(abstract_feature_type& feature);


        std::vector<std::reference_wrapper<abstract_feature_type>> _features;

    };

    template <unsigned int D, typename FeatureType, typename Scalar>
class FeatureGroup {

};
}// namespace balsa::visualization::vulkan
