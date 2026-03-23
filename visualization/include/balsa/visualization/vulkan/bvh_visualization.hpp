// bvh_visualization.hpp — BVH bounding volume visualization
//
// Provides:
//   - BVHOverlay: manages BVH construction and wireframe visualization
//     as scene graph objects within a MeshScene.
//
// Requires quiver (KDOP, BVH).

#if !defined(BALSA_VISUALIZATION_VULKAN_BVH_VISUALIZATION_HPP)
#define BALSA_VISUALIZATION_VULKAN_BVH_VISUALIZATION_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stack>
#include <string>
#include <vector>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/MeshData.hpp>

#include <quiver/spatial/KDOP.hpp>
#include <quiver/spatial/KDOPGeometry.hpp>
#include <quiver/spatial/BVH.hpp>

namespace balsa::visualization::vulkan {

// ── Collect BVH node bounds at a given tree depth ───────────────────

template<int8_t SpatialDim, int8_t K, typename DirPolicy>
auto collect_bounds_at_depth(
  const quiver::spatial::BVH<SpatialDim, K, DirPolicy> &bvh,
  int target_depth)
  -> std::vector<quiver::spatial::KDOP<SpatialDim, K, DirPolicy>> {

    using kdop_type = quiver::spatial::KDOP<SpatialDim, K, DirPolicy>;
    std::vector<kdop_type> result;

    if (!bvh.is_built() || bvh.node_count() == 0) return result;

    auto nodes = bvh.nodes();

    std::stack<std::pair<uint32_t, int>> stack;
    stack.push({ 0, 0 });

    while (!stack.empty()) {
        auto [idx, depth] = stack.top();
        stack.pop();

        const auto &node = nodes[idx];

        if (depth == target_depth || node.is_leaf()) {
            result.push_back(node.bounds);
        } else {
            stack.push({ node.right_child(), depth + 1 });
            stack.push({ idx + 1, depth + 1 });// left child (implicit)
        }
    }

    return result;
}

// ── Convert KDOP geometry into MeshData for wireframe rendering ─────

template<int8_t K, typename DirPolicy>
void set_kdop_wireframe(
  scene_graph::MeshData &mesh_data,
  const std::vector<quiver::spatial::KDOP<3, K, DirPolicy>> &bounds,
  float uniform_color[4]) {

    std::vector<scene_graph::Vec3f> all_positions;
    std::vector<uint32_t> all_edges;

    for (const auto &kdop : bounds) {
        auto geom = quiver::spatial::kdop_geometry(kdop);

        uint32_t base = static_cast<uint32_t>(all_positions.size());

        for (const auto &v : geom.vertices) {
            scene_graph::Vec3f pos;
            pos(0) = static_cast<float>(v[0]);
            pos(1) = static_cast<float>(v[1]);
            pos(2) = static_cast<float>(v[2]);
            all_positions.push_back(pos);
        }

        for (const auto &[a, b] : geom.edges) {
            all_edges.push_back(base + static_cast<uint32_t>(a));
            all_edges.push_back(base + static_cast<uint32_t>(b));
        }
    }

    mesh_data.set_positions(all_positions);
    mesh_data.set_edge_indices(all_edges);

    auto &rs = mesh_data.render_state();
    rs.layers.solid.enabled = false;
    rs.layers.wireframe.enabled = true;
    rs.layers.wireframe.color[0] = uniform_color[0];
    rs.layers.wireframe.color[1] = uniform_color[1];
    rs.layers.wireframe.color[2] = uniform_color[2];
    rs.layers.wireframe.color[3] = uniform_color[3];
    rs.layers.points.enabled = false;
    rs.color_source = ColorSource::Uniform;
    rs.uniform_color[0] = uniform_color[0];
    rs.uniform_color[1] = uniform_color[1];
    rs.uniform_color[2] = uniform_color[2];
    rs.uniform_color[3] = uniform_color[3];
    rs.normal_source = NormalSource::None;
    rs.shading = ShadingModel::Flat;
}

// ── BVHOverlay ──────────────────────────────────────────────────────
//
// Manages BVH construction over a triangle mesh and renders bounding
// volumes as wireframe overlays in the scene graph.  Attaches to an
// existing MeshScene — call set_mesh_data() after loading a mesh, then
// draw_panel() each frame for ImGui controls.

class BVHOverlay {
  public:
    BVHOverlay() = default;

    // Provide the triangle mesh data for BVH construction.
    // positions: Vec3f per vertex, tri_indices: 3 uint32_t per triangle.
    void set_mesh_data(
      const std::vector<scene_graph::Vec3f> &positions,
      const std::vector<uint32_t> &tri_indices) {

        _n_verts = positions.size();
        _n_tris = tri_indices.size() / 3;

        _vertices_d.resize(_n_verts);
        for (size_t j = 0; j < _n_verts; ++j) {
            _vertices_d[j] = {
                static_cast<double>(positions[j](0)),
                static_cast<double>(positions[j](1)),
                static_cast<double>(positions[j](2))
            };
        }

        _tri_indices = tri_indices;
        _has_data = true;

        rebuild_bvh();
    }

    // Clear all BVH data and remove overlay from scene.
    void clear(MeshScene &scene) {
        remove_overlay(scene);
        _has_data = false;
        _bvh_height = -1;
        _bvh_3 = {};
        _bvh_9 = {};
        _bvh_13 = {};
        _vertices_d.clear();
        _tri_indices.clear();
    }

    // Update the bounding volume wireframe overlay in the scene.
    // Call after any parameter change (depth, k, color).
    void update_overlay(MeshScene &scene) {
        remove_overlay(scene);

        if (!_has_data || _bvh_height < 0 || !_enabled) return;

        auto &obj = scene.add_mesh("BVH Level " + std::to_string(_display_depth));
        _bv_obj = &obj;
        auto *md = obj.find_feature<scene_graph::MeshData>();

        if (_kdop_k == 3) {
            auto bounds = collect_bounds_at_depth(_bvh_3, _display_depth);
            set_kdop_wireframe(*md, bounds, _bv_color);
        } else if (_kdop_k == 9) {
            auto bounds = collect_bounds_at_depth(_bvh_9, _display_depth);
            set_kdop_wireframe(*md, bounds, _bv_color);
        } else {
            auto bounds = collect_bounds_at_depth(_bvh_13, _display_depth);
            set_kdop_wireframe(*md, bounds, _bv_color);
        }
    }

    // Draw the ImGui BVH controls panel.  Returns true if anything changed.
    bool draw_panel(MeshScene &scene) {
        if (!_show_panel) return false;

        bool changed = false;

        if (ImGui::Begin("BVH Bounding Volumes", &_show_panel)) {
            if (!_has_data) {
                ImGui::TextDisabled("Load a mesh to visualize BVH.");
                ImGui::End();
                return false;
            }

            // Enable/disable toggle
            if (ImGui::Checkbox("Show BVH Overlay", &_enabled)) {
                update_overlay(scene);
                changed = true;
            }

            if (!_enabled) {
                ImGui::End();
                return changed;
            }

            ImGui::Separator();

            // KDOP type selector
            ImGui::Text("Bounding Volume Type:");
            int prev_k = _kdop_k;
            ImGui::RadioButton("AABB (K=3)", &_kdop_k, 3);
            ImGui::SameLine();
            ImGui::RadioButton("9-DOP", &_kdop_k, 9);
            ImGui::SameLine();
            ImGui::RadioButton("13-DOP", &_kdop_k, 13);

            if (_kdop_k != prev_k) {
                rebuild_bvh();
                update_overlay(scene);
                changed = true;
            }

            ImGui::Separator();

            // Tree depth slider
            ImGui::Text("BVH height: %d", _bvh_height);
            int prev_depth = _display_depth;
            ImGui::SliderInt("Display depth", &_display_depth, 0, _bvh_height);
            if (_display_depth != prev_depth) {
                update_overlay(scene);
                changed = true;
            }

            // Count of volumes at current level
            size_t bv_count = 0;
            if (_kdop_k == 3)
                bv_count = collect_bounds_at_depth(_bvh_3, _display_depth).size();
            else if (_kdop_k == 9)
                bv_count = collect_bounds_at_depth(_bvh_9, _display_depth).size();
            else
                bv_count = collect_bounds_at_depth(_bvh_13, _display_depth).size();
            ImGui::Text("Bounding volumes at depth %d: %zu", _display_depth, bv_count);

            ImGui::Separator();

            // Appearance controls
            if (ImGui::ColorEdit4("BV Color", _bv_color)) {
                update_overlay(scene);
                changed = true;
            }
        }
        ImGui::End();

        return changed;
    }

    // Access panel visibility (for View menu integration).
    bool &show_panel() { return _show_panel; }

  private:
    // Mesh data (doubles for KDOP fitting)
    bool _has_data = false;
    size_t _n_verts = 0;
    size_t _n_tris = 0;
    std::vector<std::array<double, 3>> _vertices_d;
    std::vector<uint32_t> _tri_indices;

    // BVH state
    int _kdop_k = 3;
    quiver::spatial::BVH<3, 3> _bvh_3;
    quiver::spatial::BVH<3, 9> _bvh_9;
    quiver::spatial::BVH<3, 13> _bvh_13;
    int _bvh_height = -1;

    // Visualization state
    bool _enabled = true;
    bool _show_panel = true;
    int _display_depth = 0;
    float _bv_color[4] = { 0.1f, 0.8f, 0.2f, 1.0f };

    // Scene graph reference (not owned)
    scene_graph::Object *_bv_obj = nullptr;

    void remove_overlay(MeshScene &scene) {
        if (_bv_obj) {
            scene.remove_mesh(_bv_obj);
            _bv_obj = nullptr;
        }
    }

    template<int8_t K>
    auto build_per_triangle_kdops()
      -> std::vector<quiver::spatial::KDOP<3, K>> {

        using kdop_type = quiver::spatial::KDOP<3, K>;
        std::vector<kdop_type> bounds(_n_tris, kdop_type::empty());

        for (size_t t = 0; t < _n_tris; ++t) {
            uint32_t i0 = _tri_indices[t * 3 + 0];
            uint32_t i1 = _tri_indices[t * 3 + 1];
            uint32_t i2 = _tri_indices[t * 3 + 2];

            std::array<std::span<const double, 3>, 3> pts = {
                std::span<const double, 3>(_vertices_d[i0]),
                std::span<const double, 3>(_vertices_d[i1]),
                std::span<const double, 3>(_vertices_d[i2])
            };

            bounds[t] = kdop_type::fit(pts);
        }

        return bounds;
    }

    void rebuild_bvh() {
        if (!_has_data || _n_tris == 0) return;

        quiver::spatial::BVHConfig config;
        config.strategy = quiver::spatial::BVHBuildStrategy::sah;
        config.max_leaf_size = 4;

        if (_kdop_k == 3) {
            auto bounds = build_per_triangle_kdops<3>();
            _bvh_3.build(bounds, config);
            _bvh_height = _bvh_3.height();
            spdlog::info("Built AABB BVH: {} nodes, height {}",
                         _bvh_3.node_count(),
                         _bvh_height);
        } else if (_kdop_k == 9) {
            auto bounds = build_per_triangle_kdops<9>();
            _bvh_9.build(bounds, config);
            _bvh_height = _bvh_9.height();
            spdlog::info("Built 9-DOP BVH: {} nodes, height {}",
                         _bvh_9.node_count(),
                         _bvh_height);
        } else {
            auto bounds = build_per_triangle_kdops<13>();
            _bvh_13.build(bounds, config);
            _bvh_height = _bvh_13.height();
            spdlog::info("Built 13-DOP BVH: {} nodes, height {}",
                         _bvh_13.node_count(),
                         _bvh_height);
        }

        _display_depth = std::min(_display_depth, _bvh_height);
    }
};

}// namespace balsa::visualization::vulkan

#endif
