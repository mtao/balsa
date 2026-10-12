#pragma once
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
    auto draw_image_controls(ImageScene &scene, ImagePanelState &state) -> bool;

} // namespace imgui
} // namespace balsa::visualization::vulkan
