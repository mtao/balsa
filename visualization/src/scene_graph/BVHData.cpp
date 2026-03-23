#include "balsa/scene_graph/BVHData.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/Object.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <stack>
#include <string>
#include <vector>

#include <quiver/Mesh.hpp>
#include <quiver/spatial/bounding_volume.hpp>

#include <spdlog/spdlog.h>

namespace balsa::scene_graph {

// ── Collect BVH node bounds at a given tree depth ───────────────────

template<int8_t SpatialDim, int8_t K, typename DirPolicy>
static auto collect_bounds_at_depth(
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

// ── Set wireframe geometry from KDOP bounds ─────────────────────────

template<int8_t K, typename DirPolicy>
static void set_kdop_wireframe(
  MeshData &mesh_data,
  const std::vector<quiver::spatial::KDOP<3, K, DirPolicy>> &bounds,
  float uniform_color[4]) {

    std::vector<Vec3f> all_positions;
    std::vector<uint32_t> all_edges;

    for (const auto &kdop : bounds) {
        auto geom = quiver::spatial::kdop_geometry(kdop);

        uint32_t base = static_cast<uint32_t>(all_positions.size());

        for (const auto &v : geom.vertices) {
            Vec3f pos;
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
    rs.color_source = visualization::vulkan::ColorSource::Uniform;
    rs.uniform_color[0] = uniform_color[0];
    rs.uniform_color[1] = uniform_color[1];
    rs.uniform_color[2] = uniform_color[2];
    rs.uniform_color[3] = uniform_color[3];
    rs.normal_source = visualization::vulkan::NormalSource::None;
    rs.shading = visualization::vulkan::ShadingModel::Flat;
}

// ── Build quiver::Mesh<2> from MeshData positions + triangles ───────
//
// Converts MeshData's flat float positions (Vec3f) and uint32_t
// triangle indices into the quiver::Mesh<2> representation needed by
// quiver::spatial::make_bvh.

using Vec3d = std::array<double, 3>;

static auto make_quiver_mesh(
  std::span<const Vec3f> positions,
  std::span<const uint32_t> tri_indices)
  -> std::pair<quiver::Mesh<2>,
               quiver::attributes::AttributeHandle<const Vec3d>> {

    // Convert uint32_t triples → int64_t triples for from_vertex_indices.
    size_t n_tris = tri_indices.size() / 3;
    std::vector<std::array<int64_t, 3>> tris(n_tris);
    for (size_t t = 0; t < n_tris; ++t) {
        tris[t] = {
            static_cast<int64_t>(tri_indices[t * 3 + 0]),
            static_cast<int64_t>(tri_indices[t * 3 + 1]),
            static_cast<int64_t>(tri_indices[t * 3 + 2])
        };
    }

    auto mesh = quiver::Mesh<2>::from_vertex_indices(tris);

    // Create a vertex position attribute (double, as required by KDOP).
    auto pos_handle = mesh.create_attribute<Vec3d>("positions", 0);
    for (size_t i = 0; i < positions.size(); ++i) {
        pos_handle[i] = {
            static_cast<double>(positions[i](0)),
            static_cast<double>(positions[i](1)),
            static_cast<double>(positions[i](2))
        };
    }

    // Return const handle for make_bvh.
    quiver::attributes::AttributeHandle<const Vec3d> cpos(pos_handle);
    return { std::move(mesh), cpos };
}

// ── BVHData::rebuild_bvh ────────────────────────────────────────────

void BVHData::rebuild_bvh(MeshData &mesh_data) {
    auto positions = mesh_data.positions();
    auto tri_indices = mesh_data.triangle_indices();

    if (positions.empty() || tri_indices.empty()) {
        _bvh_height = -1;
        return;
    }

    auto [mesh, pos] = make_quiver_mesh(positions, tri_indices);

    quiver::spatial::BVHConfig config;
    config.strategy = strategy;
    config.max_leaf_size = max_leaf_size;

    if (kdop_k == 3) {
        _bvh_3 = quiver::spatial::make_bvh<2, 3, 3>(mesh, pos, config);
        _bvh_height = _bvh_3.height();
        spdlog::info("Built AABB BVH: {} nodes, height {}",
                     _bvh_3.node_count(),
                     _bvh_height);
    } else if (kdop_k == 9) {
        _bvh_9 = quiver::spatial::make_bvh<2, 3, 9>(mesh, pos, config);
        _bvh_height = _bvh_9.height();
        spdlog::info("Built 9-DOP BVH: {} nodes, height {}",
                     _bvh_9.node_count(),
                     _bvh_height);
    } else {
        _bvh_13 = quiver::spatial::make_bvh<2, 3, 13>(mesh, pos, config);
        _bvh_height = _bvh_13.height();
        spdlog::info("Built 13-DOP BVH: {} nodes, height {}",
                     _bvh_13.node_count(),
                     _bvh_height);
    }

    display_depth = std::min(display_depth, _bvh_height);
}

// ── BVHData::update_overlay ─────────────────────────────────────────

void BVHData::update_overlay() {
    remove_overlay();

    if (_bvh_height < 0 || !enabled) return;

    // Create a child Object on the owning mesh Object.
    auto &parent = object();
    auto &child = parent.add_child(
      "BVH K=" + std::to_string(kdop_k) + " d=" + std::to_string(display_depth));
    child.selectable = false;// not user-selectable in the outliner
    _overlay_obj = &child;

    auto &md = child.emplace_feature<MeshData>();

    if (kdop_k == 3) {
        auto bounds = collect_bounds_at_depth(_bvh_3, display_depth);
        set_kdop_wireframe(md, bounds, color);
    } else if (kdop_k == 9) {
        auto bounds = collect_bounds_at_depth(_bvh_9, display_depth);
        set_kdop_wireframe(md, bounds, color);
    } else {
        auto bounds = collect_bounds_at_depth(_bvh_13, display_depth);
        set_kdop_wireframe(md, bounds, color);
    }
}

// ── BVHData::remove_overlay ─────────────────────────────────────────

void BVHData::remove_overlay() {
    if (_overlay_obj) {
        auto detached = _overlay_obj->detach();
        // unique_ptr goes out of scope → child destroyed
        _overlay_obj = nullptr;
    }
}

// ── BVHData::apply_pending_update ───────────────────────────────────

bool BVHData::apply_pending_update(MeshData &mesh_data) {
    if (!_needs_update) return false;
    _needs_update = false;

    if (_needs_rebuild) {
        rebuild_bvh(mesh_data);
        _needs_rebuild = false;
    }

    update_overlay();
    return true;
}

}// namespace balsa::scene_graph
