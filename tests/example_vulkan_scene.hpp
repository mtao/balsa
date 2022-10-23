#if defined(SKIP_ALL)
#else
#include <balsa/visualization//vulkan//film.hpp>
#include <vulkan/vulkan.hpp>
#include <optional>
#include <balsa/visualization/vulkan/scene.hpp>
#include <balsa/scene_graph/transformations/matrix_transformation.hpp>
#include <balsa/scene_graph/embedding_traits.hpp>

class BasicImGuiScene: public balsa::visualization::vulkan::SceneBase {
  public:
    // HelloTriangleScene(balsa::visualization::vulkan::Film &film);
    BasicImGuiScene();
    ~BasicImGuiScene();


    void initialize(balsa::visualization::vulkan::Film& film);

    void draw(balsa::visualization::vulkan::Film &film);

    void begin_render_pass(balsa::visualization::vulkan::Film& film) override;


  private:
    void create_graphics_pipeline(balsa::visualization::vulkan::Film &film);

    vk::Device device;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;
};

class HelloTriangleScene : public balsa::visualization::vulkan::Scene<balsa::scene_graph::transformations::MatrixTransformation<balsa::scene_graph::embedding_traits3F>> {
    using embedding_traits = balsa::scene_graph::embedding_traits3F;
    using transformation_type = balsa::scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = balsa::visualization::vulkan::Scene<transformation_type>;

  public:
    // HelloTriangleScene(balsa::visualization::vulkan::Film &film);
    HelloTriangleScene();
    ~HelloTriangleScene();


    void initialize(balsa::visualization::vulkan::Film& film);

    void draw(balsa::visualization::vulkan::Film &film);

    void begin_render_pass(balsa::visualization::vulkan::Film& film) override;


  private:
    void create_graphics_pipeline(balsa::visualization::vulkan::Film &film);

    vk::Device device;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;
};
#endif
