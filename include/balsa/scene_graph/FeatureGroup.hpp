#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP

#include <vector>
#include "AbstractFeature.hpp"
#include "EmbeddingTraits.hpp"

namespace balsa::visualization::scene_graph{


    template <concepts::EmbeddingTraits EmbeddingTraits>
    class AbstractFeatureGroup {
        public:
        using scalar_type = Scalar;
        constexpr static int dimension = D;
        using AbstractFeature_type = AbstractFeature<D,Scalar>;

        private:
        template<int, typename,typename> friend class FeatureGroup;
        void add(AbstractFeature_type& feature);
        void remove(AbstractFeature_type& feature);


        std::vector<std::reference_wrapper<AbstractFeature_type>> _features;

    };

    template <unsigned int D, typename FeatureType, typename Scalar>
class FeatureGroup {

};
}// namespace balsa::visualization::vulkan
