#include "balsa/geometry/lua/bindings.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <spdlog/spdlog.h>

#include <quiver/Mesh.hpp>
#include <quiver/MeshBase.hpp>
#include <quiver/attributes/AttributeHandle.hpp>

#include <balsa/geometry/BoundingBox.hpp>
#include <balsa/geometry/bounding_box.hpp>
#include <balsa/geometry/simplex/volume.hpp>
#include <balsa/geometry/simplicial_complex/boundaries.hpp>
#include <balsa/geometry/simplicial_complex/volumes.hpp>
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/geometry/winding_number.hpp>

#include <zipper/Vector.hpp>

#include <array>
#include <memory>

namespace balsa::geometry::lua {

namespace {

    // Helper: extract vertex positions from a mesh as a flat vector of
    // array<double,3>.  Returns empty vector on failure.
    auto get_positions(quiver::MeshBase &mesh)
        -> std::vector<std::array<double, 3>> {
        auto handle = mesh.get_attribute_handle<std::array<double, 3>>(
            "vertex_positions", 0);
        if (!handle) return {};

        std::vector<std::array<double, 3>> result(handle->size());
        for (std::size_t i = 0; i < handle->size(); ++i) {
            result[i] = (*handle)[i];
        }
        return result;
    }

    // Bind BoundingBox<3> as a Lua usertype.
    auto bind_bounding_box(sol::table &tbl) -> void {
        tbl.new_usertype<BoundingBox<3>>(
            "BoundingBox3",
            sol::no_constructor,
            "min",
            [](const BoundingBox<3> &bb)
                -> sol::as_table_t<std::array<double, 3>> {
                auto m = bb.min();
                return sol::as_table(std::array<double, 3>{m(0), m(1), m(2)});
            },
            "max",
            [](const BoundingBox<3> &bb)
                -> sol::as_table_t<std::array<double, 3>> {
                auto m = bb.max();
                return sol::as_table(std::array<double, 3>{m(0), m(1), m(2)});
            },
            "is_empty",
            &BoundingBox<3>::is_empty);
    }

    // geometry.bounding_box(mesh) -> BoundingBox3
    // Computes axis-aligned bounding box from vertex_positions.
    auto bind_bounding_box_fn(sol::table &tbl) -> void {
        tbl.set_function(
            "bounding_box",
            [](quiver::MeshBase &mesh) -> std::optional<BoundingBox<3>> {
                auto pos = get_positions(mesh);
                if (pos.empty()) return std::nullopt;

                BoundingBox<3> bb;
                for (const auto &p : pos) {
                    zipper::Vector<double, 3> v{p[0], p[1], p[2]};
                    bb.expand(v);
                }
                return bb;
            });
    }

    // geometry.read_obj(path) -> mesh, err
    // Reads an OBJ file and returns a quiver TriMesh.
    auto bind_read_obj(sol::state_view lua, sol::table &tbl) -> void {
        tbl.set_function(
            "read_obj",
            [&lua](const std::string &path)
                -> std::tuple<sol::object, sol::object> {
                try {
                    auto obj_data =
                        balsa::geometry::triangle_mesh::read_objD(path);

                    // Build a quiver TriMesh from the OBJ data.
                    // The OBJ reader returns separate position/normal
                    // TriangleMesh structs.  We construct a quiver mesh
                    // from the position mesh's triangles.
                    const auto &pmesh = obj_data.position;

                    // Create a TriMesh via quiver's from_vertex_indices.
                    std::size_t n_verts = pmesh.vertices.cols();
                    std::size_t n_tris = pmesh.triangles.cols();

                    if (n_verts == 0 || n_tris == 0) {
                        return {
                            sol::nil,
                            sol::make_object(lua, "Empty mesh in OBJ file")};
                    }

                    // Build triangle index array for quiver
                    // (from_vertex_indices expects int64_t).
                    std::vector<std::array<int64_t, 3>> tri_indices(n_tris);
                    for (std::size_t t = 0; t < n_tris; ++t) {
                        for (int k = 0; k < 3; ++k) {
                            tri_indices[t][static_cast<std::size_t>(k)] =
                                static_cast<int64_t>(pmesh.triangles(k, t));
                        }
                    }

                    auto mesh =
                        quiver::Mesh<2>::from_vertex_indices(tri_indices);
                    auto mesh_ptr =
                        std::make_shared<quiver::Mesh<2>>(std::move(mesh));

                    // Add vertex positions as an attribute.
                    using arr3 = std::array<double, 3>;
                    auto pos_handle =
                        mesh_ptr->create_attribute<arr3>("vertex_positions", 0);
                    pos_handle.attribute().resize(n_verts);
                    for (std::size_t i = 0; i < n_verts; ++i) {
                        pos_handle[i] = {pmesh.vertices(0, i),
                                         pmesh.vertices(1, i),
                                         pmesh.vertices(2, i)};
                    }

                    // Add vertex normals if present.
                    const auto &nmesh = obj_data.normal;
                    if (static_cast<std::size_t>(nmesh.vertices.cols())
                        == n_verts) {
                        auto norm_handle = mesh_ptr->create_attribute<arr3>(
                            "vertex_normals", 0);
                        norm_handle.attribute().resize(n_verts);
                        for (std::size_t i = 0; i < n_verts; ++i) {
                            norm_handle[i] = {nmesh.vertices(0, i),
                                              nmesh.vertices(1, i),
                                              nmesh.vertices(2, i)};
                        }
                    }

                    // Return as MeshBase shared_ptr so Lua sees the right
                    // metatable (quiver's Lua bindings register Mesh<2>
                    // as "TriMesh" with MeshBase inheritance).
                    std::shared_ptr<quiver::MeshBase> base_ptr = mesh_ptr;
                    return {sol::make_object(lua, base_ptr), sol::nil};

                } catch (const std::exception &e) {
                    return {sol::nil, sol::make_object(lua, e.what())};
                }
            });
    }

} // anonymous namespace

auto load_bindings(sol::state_view lua) -> void {
    auto tbl = lua["geometry"].get_or_create<sol::table>();

    // Guard against double-registration.
    if (tbl["BoundingBox3"].valid()) return;

    bind_bounding_box(tbl);
    bind_bounding_box_fn(tbl);
    bind_read_obj(lua, tbl);

    spdlog::debug("balsa::geometry Lua bindings loaded");
}

} // namespace balsa::geometry::lua
