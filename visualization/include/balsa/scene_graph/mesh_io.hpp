#if !defined(BALSA_SCENE_GRAPH_MESH_IO_HPP)
#define BALSA_SCENE_GRAPH_MESH_IO_HPP

#include <expected>
#include <filesystem>
#include <string>

namespace balsa::scene_graph {

class MeshData;

// Load a mesh file into MeshData (replacing its contents).
//
// Supported formats: OBJ (via balsa::geometry).  Adds
// "vertex_positions" (and "vertex_normals" when the file's normals are
// indexed identically to its positions), plus triangle indices.
//
// Richer formats (e.g. MSH) belong to libraries that build on balsa
// and fill MeshData through its attribute API.
auto load_mesh(MeshData &mesh_data, const std::filesystem::path &path)
    -> std::expected<void, std::string>;

} // namespace balsa::scene_graph

#endif
