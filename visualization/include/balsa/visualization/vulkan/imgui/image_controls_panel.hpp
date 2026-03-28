#if !defined(BALSA_VISUALIZATION_VULKAN_IMGUI_IMAGE_CONTROLS_PANEL_HPP)
#define BALSA_VISUALIZATION_VULKAN_IMGUI_IMAGE_CONTROLS_PANEL_HPP

namespace balsa::visualization::vulkan {

class ImageScene;

namespace imgui {

    // Persistent state for the image controls panel.
    // Caller owns this and passes it to draw_image_controls() each frame.
    struct ImagePanelState {
        bool show_controls = true;
    };

    // Draw the image display controls panel (exposure, gamma, channel
    // mode, zoom/pan, fit-to-window button, image info).
    //
    // Assumes ImGui::NewFrame() has been called and the caller will
    // call ImGui::Render() afterward.
    //
    // Returns true if any value was modified this frame.
    bool draw_image_controls(ImageScene &scene, ImagePanelState &state);

} // namespace imgui
} // namespace balsa::visualization::vulkan

#endif
