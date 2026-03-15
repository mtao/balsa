#if defined(SKIP_ALL)
#else
#include <balsa/visualization/vulkan/film.hpp>
#include <balsa/visualization/vulkan/imgui_integration.hpp>
#include <vulkan/vulkan.hpp>
#include <optional>
#include <balsa/visualization/vulkan/scene.hpp>
#include <balsa/scene_graph/transformations/matrix_transformation.hpp>
#include <balsa/scene_graph/embedding_traits.hpp>

// A simple demo scene that renders a colormap-cycling triangle with an
// ImGui overlay (the demo window).  ImGui is optional — if init_imgui()
// is never called the scene renders just the triangle.
class BasicImGuiScene : public balsa::visualization::vulkan::SceneBase {
  public:
    BasicImGuiScene();
    ~BasicImGuiScene();

    void initialize(balsa::visualization::vulkan::Film &film) override;
    void draw(balsa::visualization::vulkan::Film &film) override;
    void begin_render_pass(balsa::visualization::vulkan::Film &film) override;

    // Call after the Film is initialized (render pass exists) but before exec().
    // If glfw_window is non-null, the GLFW platform backend (input, clipboard)
    // is also set up.  Ownership of glfw_window remains with the caller.
    void init_imgui(balsa::visualization::vulkan::Film &film,
                    GLFWwindow *glfw_window = nullptr);

  private:
    void create_graphics_pipeline(balsa::visualization::vulkan::Film &film);

    vk::Device device;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;

    balsa::visualization::vulkan::ImGuiIntegration _imgui;
    bool _show_demo_window = true;
};

class HelloTriangleScene : public balsa::visualization::vulkan::Scene<balsa::scene_graph::transformations::MatrixTransformation<balsa::scene_graph::embedding_traits3F>> {
    using embedding_traits = balsa::scene_graph::embedding_traits3F;
    using transformation_type = balsa::scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = balsa::visualization::vulkan::Scene<transformation_type>;

  public:
    HelloTriangleScene();
    ~HelloTriangleScene();

    void initialize(balsa::visualization::vulkan::Film &film);
    void draw(balsa::visualization::vulkan::Film &film);
    void begin_render_pass(balsa::visualization::vulkan::Film &film) override;

  private:
    void create_graphics_pipeline(balsa::visualization::vulkan::Film &film);

    vk::Device device;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;
};
#endif
