#include "balsa/visualization/vulkan/imgui/image_controls_panel.hpp"
#include "balsa/scene_graph/ImageData.hpp"
#include "balsa/visualization/vulkan/image_scene.hpp"

#include <cstdio>
#include <imgui.h>

namespace balsa::visualization::vulkan::imgui {

auto draw_image_controls(ImageScene &scene, ImagePanelState &state) -> bool {
    bool changed = false;

    if (!state.show_controls) return false;

    if (ImGui::Begin("Image Controls", &state.show_controls)) {
        auto *img = scene.image_data();

        // ── Image info ──────────────────────────────────────────────
        if (img && img->has_pixels()) {
            ImGui::Text("Size: %u x %u", img->width(), img->height());
            const char *fmt_name =
                (img->format() == scene_graph::ImageData::Format::RGBAF32)
                    ? "Float32 HDR"
                    : "RGBA8 sRGB";
            ImGui::Text("Format: %s", fmt_name);
            ImGui::Separator();
        } else {
            ImGui::TextDisabled("No image loaded");
            ImGui::End();
            return false;
        }

        // ── Tone Mapping ────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Tone Mapping",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            float exposure = img->exposure();
            if (ImGui::SliderFloat("Exposure (EV)", &exposure, -10.0f, 10.0f)) {
                img->set_exposure(exposure);
                changed = true;
            }

            float gamma = img->gamma();
            if (ImGui::SliderFloat("Gamma", &gamma, 0.1f, 5.0f)) {
                img->set_gamma(gamma);
                changed = true;
            }

            if (ImGui::Button("Reset Tone Mapping")) {
                img->set_exposure(0.0f);
                img->set_gamma(2.2f);
                changed = true;
            }
        }

        // ── Channel Mode ────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Channel Display",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            static constexpr const char *channel_names[] = {
                "RGBA", "Red", "Green", "Blue", "Alpha", "Luminance"};
            int current = static_cast<int>(img->channel_mode());
            if (ImGui::Combo("Channel", &current, channel_names, 6)) {
                img->set_channel_mode(
                    static_cast<scene_graph::ImageData::ChannelMode>(current));
                changed = true;
            }
        }

        // ── Navigation ──────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Navigation",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            float zoom = scene.zoom();
            if (ImGui::SliderFloat("Zoom",
                                   &zoom,
                                   0.01f,
                                   50.0f,
                                   "%.2fx",
                                   ImGuiSliderFlags_Logarithmic)) {
                scene.set_zoom(zoom);
                changed = true;
            }

            float pan_x = scene.pan_x();
            float pan_y = scene.pan_y();
            bool pan_changed = false;
            pan_changed |= ImGui::SliderFloat("Pan X", &pan_x, -2.0f, 2.0f);
            pan_changed |= ImGui::SliderFloat("Pan Y", &pan_y, -2.0f, 2.0f);
            if (pan_changed) {
                scene.set_pan(pan_x, pan_y);
                changed = true;
            }

            if (ImGui::Button("Fit to Window")) {
                scene.fit_to_window();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("1:1 (100%)")) {
                scene.set_zoom(1.0f);
                scene.set_pan(0.0f, 0.0f);
                changed = true;
            }
        }
    }
    ImGui::End();

    return changed;
}

} // namespace balsa::visualization::vulkan::imgui
