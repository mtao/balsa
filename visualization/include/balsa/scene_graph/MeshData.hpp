#if !defined(BALSA_SCENE_GRAPH_MESH_DATA_HPP)
#define BALSA_SCENE_GRAPH_MESH_DATA_HPP

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

#include "AbstractFeature.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "types.hpp"

#include <quiver/MeshBase.hpp>
#include <quiver/attributes/AttributeHandle.hpp>
#include <quiver/attributes/StoredAttributeBase.hpp>

namespace balsa::scene_graph {

// ── DiscoveredAttribute ─────────────────────────────────────────────
//
// Metadata about a single attribute found in the source MeshBase's
// AttributeManager.  Populated by MeshData::set_mesh() and exposed
// for UI enumeration (attribute selector combos, property panels).

struct DiscoveredAttribute {
    quiver::attributes::ConstTypeErasedAttributeHandle handle;
    std::string name; // from AttributeManager::name_of()
    int8_t dimension = -1; // quiver dim (0=vertex, 1=edge, 2=face, ...)
    std::type_index type_id; // from StoredAttributeBase::value_type_id()
    std::size_t element_size = 0; // bytes per element
    std::size_t count = 0; // number of elements
    uint8_t component_count =
        0; // 1 for scalar, 2 for array<T,2>, 3 for array<T,3>, ...
    bool is_floating_point = false; // true for float/double element types

    DiscoveredAttribute() : type_id(typeid(void)) {}
};

// ── RoleBinding ─────────────────────────────────────────────────────
//
// Binds a visualization role (positions, normals, scalar field) to
// an attribute from the source mesh.
//
// When the source attribute is double-typed, a CachedAttribute
// (TransformAttribute → StoredAttribute<float>) is created to
// provide GPU-ready float data.  When the source is already float,
// gpu_ready points directly at the source — zero copy.
//
// The sync layer reads raw_data() + component_count from gpu_ready
// to upload to the GPU with the correct VkFormat.

struct RoleBinding {
    // Handle to the original attribute in the source mesh.
    quiver::attributes::ConstTypeErasedAttributeHandle source;

    // Owned CachedAttribute that materialises a float conversion
    // of the source.  Null when source is already float-typed and
    // gpu_ready points directly at source.
    std::unique_ptr<quiver::attributes::StoredAttributeBase> cached;

    // Handle to whichever attribute the GPU should read from.
    // Points to cached (if present) or source.
    quiver::attributes::ConstTypeErasedAttributeHandle gpu_ready;

    // Number of float components per element in gpu_ready (1, 2, 3, 4).
    uint8_t component_count = 0;

    // Whether this role is bound to a valid attribute.
    bool is_bound() const { return gpu_ready.valid(); }

    // Number of elements in the gpu_ready attribute.
    std::size_t size() const;

    // Raw pointer to contiguous float data for GPU upload.
    // Returns nullptr if not bound.
    const void *raw_data() const;

    // Reset the binding to empty.
    void clear();
};

// ── MeshData ────────────────────────────────────────────────────────
//
// Feature that holds CPU-side mesh geometry and appearance parameters.
//
// Backed by a shared quiver::MeshBase.  All attributes from the mesh
// are enumerable and assignable to visualization roles (positions,
// normals, scalar field).  The rendering layer reads GPU-ready data
// directly from the role bindings' raw_data() pointers.
//
// Topology-derived data (triangle and edge index buffers) is
// extracted from the mesh's skeletons and stored as flat uint32_t
// arrays, since quiver's topology representation is not contiguous.
//
// The version counter is bumped on set_mesh() and role reassignment
// so the rendering layer knows when GPU buffers need re-uploading.

class MeshData : public AbstractFeature {
  public:
    using RenderState = visualization::vulkan::MeshRenderState;

    MeshData();

    // ── Mesh source ─────────────────────────────────────────────────
    //
    // Store a quiver MeshBase as the source of all attribute data.
    // Enumerates attributes, auto-assigns roles by convention name
    // (vertex_positions → position, vertex_normals → normal), builds
    // skeletons, extracts topology indices.

    void set_mesh(std::shared_ptr<quiver::MeshBase> mesh);

    const quiver::MeshBase *mesh() const { return _mesh.get(); }

    // ── Attribute discovery ─────────────────────────────────────────

    const std::vector<DiscoveredAttribute> &discovered_attributes() const {
        return _discovered;
    }

    // Re-scan the source mesh's AttributeManager for new attributes
    // added since the last set_mesh() or rediscover_attributes() call.
    // Preserves existing role bindings.  Bumps the version counter.
    void rediscover_attributes();

    // ── Role assignment ─────────────────────────────────────────────
    //
    // Bind an attribute to a visualization role.  The handle must
    // reference an attribute owned by the source mesh's
    // AttributeManager.  If the attribute type is incompatible
    // (e.g. non-numeric for positions), the binding is rejected
    // and the role is left unbound.
    //
    // For scalar roles, component selects which element of a
    // multi-component attribute to use: -1 = magnitude,
    // 0/1/2/... = specific component.

    void assign_position(
        quiver::attributes::ConstTypeErasedAttributeHandle handle);
    void assign_normal(
        quiver::attributes::ConstTypeErasedAttributeHandle handle);
    void
        assign_scalar(quiver::attributes::ConstTypeErasedAttributeHandle handle,
                      int component = -1);

    // Clear a role binding.
    void clear_position();
    void clear_normal();
    void clear_scalar();

    // ── Role accessors ──────────────────────────────────────────────

    const RoleBinding &position_binding() const { return _position; }
    const RoleBinding &normal_binding() const { return _normal; }
    const RoleBinding &scalar_binding() const { return _scalar; }

    int scalar_component() const { return _scalar_component; }

    // ── Topology index buffers ──────────────────────────────────────
    //
    // Extracted from the mesh's skeletons.  Triangle indices come
    // from Skeleton<0> (simplex-vertex connectivity); edge indices
    // come from Skeleton<1> + IncidentFaceIndices.

    std::span<const uint32_t> triangle_indices() const;
    std::span<const uint32_t> edge_indices() const;

    // ── Convenience queries ─────────────────────────────────────────

    bool has_positions() const { return _position.is_bound(); }
    bool has_normals() const { return _normal.is_bound(); }
    bool has_scalar_field() const { return _scalar.is_bound(); }
    bool has_triangle_indices() const { return !_tri_indices.empty(); }
    bool has_edge_indices() const { return !_edge_indices.empty(); }

    std::size_t vertex_count() const;
    std::size_t triangle_count() const { return _tri_indices.size() / 3; }
    std::size_t edge_count() const { return _edge_indices.size() / 2; }

    // ── Dirty tracking ──────────────────────────────────────────────

    uint64_t version() const { return _version; }

    // ── Render state ────────────────────────────────────────────────

    RenderState &render_state() { return _render_state; }
    const RenderState &render_state() const { return _render_state; }

  private:
    // Source mesh (shared ownership with the reader/caller).
    std::shared_ptr<quiver::MeshBase> _mesh;

    // All attributes discovered from the source mesh.
    std::vector<DiscoveredAttribute> _discovered;

    // Role bindings (position, normal, scalar).
    RoleBinding _position;
    RoleBinding _normal;
    RoleBinding _scalar;
    int _scalar_component = -1;

    // Flat index buffers extracted from mesh topology.
    std::vector<uint32_t> _tri_indices;
    std::vector<uint32_t> _edge_indices;

    // ── Private helpers ─────────────────────────────────────────────

    // Enumerate all attributes from the source mesh into _discovered.
    void discover_attributes();

    // Extract triangle and edge index buffers from mesh topology.
    void extract_topology_indices();

    // Build a RoleBinding for a vector-valued attribute (positions/normals).
    // Returns an unbound RoleBinding if the handle is invalid or incompatible.
    static RoleBinding build_vector_binding(
        quiver::attributes::ConstTypeErasedAttributeHandle handle);

    // Build a RoleBinding for a scalar attribute (single float per vertex).
    // Extracts a specific component from multi-component types, or magnitude.
    static RoleBinding build_scalar_binding(
        quiver::attributes::ConstTypeErasedAttributeHandle handle,
        int component);

    RenderState _render_state;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph

#endif
