#if defined(SKIP_ALL)
#else
#include <balsa/visualization//vulkan//Film.hpp>
#include <vulkan/vulkan.hpp>
#include <optional>
#include <balsa/visualization/vulkan/Scene.hpp>
#include <balsa/scene_graph/transformations/matrix_transformation.hpp>
#include <balsa/scene_graph/embedding_traits.hpp>
#include <balsa/visualization/vulkan/BufferView.hpp>
#include <balsa/visualization/vulkan/utils/VertexBufferViewCollection.hpp>
//#include <balsa/visualization/imgui//vulkan//Drawable.hpp>


class HelloTriangleScene : public balsa::visualization::vulkan::Scene<balsa::scene_graph::transformations::MatrixTransformation<balsa::scene_graph::embedding_traits3F>> {
    using embedding_traits = balsa::scene_graph::embedding_traits3F;
    using transformation_type = balsa::scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = balsa::visualization::vulkan::Scene<transformation_type>;

  public:
    // HelloTriangleScene(balsa::visualization::vulkan::Film &film);
    HelloTriangleScene();
    ~HelloTriangleScene();


    void initialize(balsa::visualization::vulkan::Film &film) override;

    void draw(balsa::visualization::vulkan::Film &film) override;

    void begin_render_pass(balsa::visualization::vulkan::Film &film) override;
    void end_render_pass(balsa::visualization::vulkan::Film &film) override;

    void toggle_mode(balsa::visualization::vulkan::Film &film);
    void toggle_buffer_mode() { use_staging_buffer ^= true; }

  private:
    void create_graphics_pipeline(balsa::visualization::vulkan::Film &film);

    // balsa::visualization::imgui::vulkan::Drawable _imgui;
    bool static_triangle_mode = false;
    bool changed = false;
    bool use_staging_buffer = false;

    vk::Device device;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;


    balsa::visualization::vulkan::utils::VertexBufferViewCollection vertex_buffer_views;
    balsa::visualization::vulkan::IndexBufferView index_buffer_view;
};
#endif
