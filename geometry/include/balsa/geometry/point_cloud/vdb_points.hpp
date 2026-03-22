#pragma once
// Lightweight zipper <-> OpenVDB PointDataGrid conversion utilities.
//
// Read/write particle attributes (positions, velocities, radii, etc.)
// between balsa's zipper column-vector types and OpenVDB's PointDataGrid.
//
// Only compiled when BALSA_HAS_OPENVDB is defined.

#include "balsa/tensor_types.hpp"
#include <openvdb/openvdb.h>
#include <openvdb/points/PointConversion.h>
#include <openvdb/points/PointCount.h>
#include <openvdb/points/PointDataGrid.h>
#include <optional>
#include <string>
#include <vector>

namespace balsa::geometry::point_cloud::vdb {

// OpenVDB's Vec3f clashes with balsa::Vec3f (zipper::Vector<float,3>) inside
// the balsa namespace.  Use a local alias to the OpenVDB type.
using OvdbVec3f = openvdb::math::Vec3<float>;

/// All particle data that can be stored in / read from a VDB PointDataGrid.
struct VDBParticleData {
    ColVecs3d positions;
    std::optional<ColVecs3d> velocities;
    std::optional<ColVecs4d> colors;
    std::optional<VecXd> radii;
    std::optional<VecXd> densities;
    std::optional<VecXi> ids;
};

// ── Writing ──────────────────────────────────────────────────────────────

/// Create a PointDataGrid from particle data.
///
/// @param data        The particle attributes to write.
/// @param voxel_size  Voxel size for the VDB transform (default 0.1).
/// @return            A PointDataGrid containing all provided attributes.
inline openvdb::points::PointDataGrid::Ptr
  create_point_data_grid(const VDBParticleData &data, double voxel_size = 0.1) {
    using namespace openvdb;
    using namespace openvdb::points;
    using namespace openvdb::math;

    const auto n = data.positions.cols();

    // Build position vector for OpenVDB
    std::vector<OvdbVec3f> vdb_positions(n);
    for (decltype(data.positions.cols()) i = 0; i < n; ++i) {
        auto p = data.positions.col(i);
        vdb_positions[i] = OvdbVec3f(static_cast<float>(p(0)),
                                     static_cast<float>(p(1)),
                                     static_cast<float>(p(2)));
    }

    // Create transform and position wrapper
    auto transform = Transform::createLinearTransform(voxel_size);

    PointAttributeVector<OvdbVec3f> pos_wrapper(vdb_positions);
    auto point_index_grid = openvdb::tools::createPointIndexGrid<tools::PointIndexGrid>(pos_wrapper, *transform);

    // Create the PointDataGrid with positions
    auto grid = createPointDataGrid<NullCodec, PointDataGrid>(
      *point_index_grid, pos_wrapper, *transform);

    // Helper: append a float attribute to every point
    auto append_float_attr = [&](const std::string &name, const VecXd &values) {
        appendAttribute<float>(grid->tree(), name);
        // Write data per-leaf
        Index64 offset = 0;
        for (auto leaf = grid->tree().beginLeaf(); leaf; ++leaf) {
            auto handle = AttributeWriteHandle<float>::create(leaf->attributeArray(name));
            for (auto idx = leaf->beginIndexOn(); idx; ++idx) {
                handle->set(*idx, static_cast<float>(values(offset)));
                ++offset;
            }
        }
    };

    // Helper: append a Vec3f attribute to every point
    auto append_vec3f_attr = [&](const std::string &name, const ColVecs3d &values) {
        appendAttribute<OvdbVec3f>(grid->tree(), name);
        Index64 offset = 0;
        for (auto leaf = grid->tree().beginLeaf(); leaf; ++leaf) {
            auto handle = AttributeWriteHandle<OvdbVec3f>::create(leaf->attributeArray(name));
            for (auto idx = leaf->beginIndexOn(); idx; ++idx) {
                auto v = values.col(offset);
                handle->set(*idx, OvdbVec3f(static_cast<float>(v(0)), static_cast<float>(v(1)), static_cast<float>(v(2))));
                ++offset;
            }
        }
    };

    // Helper: append an int attribute to every point
    auto append_int_attr = [&](const std::string &name, const VecXi &values) {
        appendAttribute<int32_t>(grid->tree(), name);
        Index64 offset = 0;
        for (auto leaf = grid->tree().beginLeaf(); leaf; ++leaf) {
            auto handle = AttributeWriteHandle<int32_t>::create(leaf->attributeArray(name));
            for (auto idx = leaf->beginIndexOn(); idx; ++idx) {
                handle->set(*idx, static_cast<int32_t>(values(offset)));
                ++offset;
            }
        }
    };

    if (data.velocities) {
        append_vec3f_attr("v", *data.velocities);
    }
    if (data.colors) {
        // Store color as vec3f "Cd" (Houdini convention) — alpha is dropped.
        // If full RGBA is needed later, a vec4f attribute could be used.
        ColVecs3d rgb(3, n);
        rgb = data.colors->topRows(3);
        append_vec3f_attr("Cd", rgb);
    }
    if (data.radii) {
        append_float_attr("pscale", *data.radii);
    }
    if (data.densities) {
        append_float_attr("density", *data.densities);
    }
    if (data.ids) {
        append_int_attr("id", *data.ids);
    }

    return grid;
}

/// Write particle data to a VDB file.
inline void write_vdb(const std::string &filename,
                      const VDBParticleData &data,
                      double voxel_size = 0.1,
                      const std::string &grid_name = "particles") {
    openvdb::initialize();
    auto grid = create_point_data_grid(data, voxel_size);
    grid->setName(grid_name);
    openvdb::io::File file(filename);
    file.write({ grid });
    file.close();
}

// ── Reading ──────────────────────────────────────────────────────────────

/// Read particle data from a VDB PointDataGrid.
inline VDBParticleData read_point_data_grid(
  const openvdb::points::PointDataGrid &grid) {
    using namespace openvdb;
    using namespace openvdb::points;

    VDBParticleData data;

    const auto total = pointCount(grid.tree());
    data.positions = ColVecs3d(3, total);

    // Determine which optional attributes exist by checking the first leaf
    bool has_velocity = false;
    bool has_color = false;
    bool has_pscale = false;
    bool has_density = false;
    bool has_id = false;

    auto leaf_iter = grid.tree().cbeginLeaf();
    if (leaf_iter) {
        const auto &descriptor = leaf_iter->attributeSet().descriptor();
        has_velocity = descriptor.find("v") != AttributeSet::INVALID_POS;
        has_color = descriptor.find("Cd") != AttributeSet::INVALID_POS;
        has_pscale = descriptor.find("pscale") != AttributeSet::INVALID_POS;
        has_density = descriptor.find("density") != AttributeSet::INVALID_POS;
        has_id = descriptor.find("id") != AttributeSet::INVALID_POS;
    }

    if (has_velocity) data.velocities = ColVecs3d(3, total);
    if (has_color) data.colors = ColVecs4d(4, total);
    if (has_pscale) data.radii = VecXd(total);
    if (has_density) data.densities = VecXd(total);
    if (has_id) data.ids = VecXi(total);

    // Iterate over leaves and extract attributes
    // Note: AttributeHandle::create() returns shared_ptr (::Ptr), not unique_ptr.
    Index64 offset = 0;
    for (auto leaf = grid.tree().cbeginLeaf(); leaf; ++leaf) {
        auto pos_handle = AttributeHandle<OvdbVec3f>::create(leaf->constAttributeArray("P"));

        // Optional handles
        typename AttributeHandle<OvdbVec3f>::Ptr vel_handle;
        typename AttributeHandle<OvdbVec3f>::Ptr cd_handle;
        typename AttributeHandle<float>::Ptr pscale_handle;
        typename AttributeHandle<float>::Ptr density_handle;
        typename AttributeHandle<int32_t>::Ptr id_handle;

        if (has_velocity)
            vel_handle = AttributeHandle<OvdbVec3f>::create(leaf->constAttributeArray("v"));
        if (has_color)
            cd_handle = AttributeHandle<OvdbVec3f>::create(leaf->constAttributeArray("Cd"));
        if (has_pscale)
            pscale_handle = AttributeHandle<float>::create(leaf->constAttributeArray("pscale"));
        if (has_density)
            density_handle = AttributeHandle<float>::create(leaf->constAttributeArray("density"));
        if (has_id)
            id_handle = AttributeHandle<int32_t>::create(leaf->constAttributeArray("id"));

        for (auto idx = leaf->beginIndexOn(); idx; ++idx) {
            // The "P" attribute stores voxel-local offsets in [0,1).
            // To reconstruct world-space positions we must:
            //   1. Get the integer voxel coordinate from the index iterator
            //   2. Add the voxel-local offset
            //   3. Transform from index space to world space
            const OvdbVec3f p_local = pos_handle->get(*idx);
            const auto ijk = idx.getCoord();
            const openvdb::Vec3d index_pos(
              ijk[0] + static_cast<double>(p_local[0]),
              ijk[1] + static_cast<double>(p_local[1]),
              ijk[2] + static_cast<double>(p_local[2]));
            const openvdb::Vec3d world_pos = grid.transform().indexToWorld(index_pos);
            data.positions.col(offset) = balsa::Vector<double, 3>{ { world_pos[0],
                                                                     world_pos[1],
                                                                     world_pos[2] } };

            if (vel_handle) {
                const OvdbVec3f v = vel_handle->get(*idx);
                data.velocities->col(offset) = balsa::Vector<double, 3>{ { static_cast<double>(v[0]),
                                                                           static_cast<double>(v[1]),
                                                                           static_cast<double>(v[2]) } };
            }
            if (cd_handle) {
                const OvdbVec3f c = cd_handle->get(*idx);
                data.colors->col(offset) = balsa::Vector<double, 4>{ {
                  static_cast<double>(c[0]),
                  static_cast<double>(c[1]),
                  static_cast<double>(c[2]),
                  1.0// alpha = 1 when reading from Cd (vec3)
                } };
            }
            if (pscale_handle) {
                (*data.radii)(offset) = static_cast<double>(pscale_handle->get(*idx));
            }
            if (density_handle) {
                (*data.densities)(offset) = static_cast<double>(density_handle->get(*idx));
            }
            if (id_handle) {
                (*data.ids)(offset) = static_cast<int>(id_handle->get(*idx));
            }
            ++offset;
        }
    }

    return data;
}

/// Read particle data from a VDB file.
///
/// @param filename   Path to the .vdb file.
/// @param grid_name  Name of the grid to read. If empty, reads the first
///                   PointDataGrid found.
/// @return           The particle data, or std::nullopt if no suitable grid
///                   was found.
inline std::optional<VDBParticleData> read_vdb(
  const std::string &filename,
  const std::string &grid_name = "") {
    using namespace openvdb;
    using namespace openvdb::points;

    openvdb::initialize();
    io::File file(filename);
    file.open();

    for (auto name_iter = file.beginName(); name_iter != file.endName(); ++name_iter) {
        if (!grid_name.empty() && name_iter.gridName() != grid_name) {
            continue;
        }
        auto base_grid = file.readGrid(name_iter.gridName());
        auto point_grid = GridBase::grid<PointDataGrid>(base_grid);
        if (point_grid) {
            auto result = read_point_data_grid(*point_grid);
            file.close();
            return result;
        }
    }

    file.close();
    return std::nullopt;
}

/// List grid names and types in a VDB file (for --info output).
inline std::vector<std::pair<std::string, std::string>> list_vdb_grids(
  const std::string &filename) {
    openvdb::initialize();
    openvdb::io::File file(filename);
    file.open();

    std::vector<std::pair<std::string, std::string>> grids;
    for (auto it = file.beginName(); it != file.endName(); ++it) {
        auto grid = file.readGrid(it.gridName());
        grids.emplace_back(it.gridName(), grid->type());
    }
    file.close();
    return grids;
}

}// namespace balsa::geometry::point_cloud::vdb
