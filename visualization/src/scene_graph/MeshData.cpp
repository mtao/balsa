#include "balsa/scene_graph/MeshData.hpp"

#include <spdlog/spdlog.h>

#include <quiver/Mesh.hpp>
#include <quiver/attributes/CachedAttribute.hpp>
#include <quiver/attributes/IncidentFaceIndices.hpp>
#include <quiver/attributes/TransformAttribute.hpp>

#include <array>
#include <cmath>
#include <typeindex>

namespace balsa::scene_graph {

namespace qattr = quiver::attributes;

// ── RoleBinding implementation ──────────────────────────────────────

std::size_t RoleBinding::size() const {
    if (!gpu_ready.valid()) return 0;
    return gpu_ready.attribute().size();
}

const void *RoleBinding::raw_data() const {
    if (!gpu_ready.valid()) return nullptr;

    // gpu_ready always points at a StoredAttribute<array<float,N>> or
    // StoredAttribute<float>.  We must dynamic_cast to the concrete type
    // to access the data vector.
    auto &attr = gpu_ready.attribute();

    // Try each float-typed StoredAttribute we could have created.
    if (auto *p = dynamic_cast<const qattr::StoredAttribute<float> *>(&attr)) {
        return p->data().data();
    }
    if (auto *p =
            dynamic_cast<const qattr::StoredAttribute<std::array<float, 2>> *>(
                &attr)) {
        return p->data().data();
    }
    if (auto *p =
            dynamic_cast<const qattr::StoredAttribute<std::array<float, 3>> *>(
                &attr)) {
        return p->data().data();
    }
    if (auto *p =
            dynamic_cast<const qattr::StoredAttribute<std::array<float, 4>> *>(
                &attr)) {
        return p->data().data();
    }
    return nullptr;
}

void RoleBinding::clear() {
    source = qattr::ConstTypeErasedAttributeHandle();
    cached.reset();
    gpu_ready = qattr::ConstTypeErasedAttributeHandle();
    component_count = 0;
}

// ── Type introspection helpers ──────────────────────────────────────
//
// Since StoredAttributeBase has NO virtual type introspection (no
// value_type_id, no element_size, no raw_data), we probe known concrete
// types via ConstTypeErasedAttributeHandle::to_handle<const T>(), which
// internally uses dynamic_cast.

namespace {

    // Result of probing a type-erased handle for its concrete type.
    struct TypeProbeResult {
        std::type_index type_id = typeid(void);
        std::size_t element_size = 0;
        uint8_t component_count = 0;
        bool is_floating_point = false;
    };

    // Probe a type-erased handle against all known attribute types.
    // Returns the first match.  Order doesn't matter for correctness.
    TypeProbeResult probe_type(qattr::ConstTypeErasedAttributeHandle handle) {
        if (!handle.valid()) return {};

        // Try float types.
        if (handle.to_handle<const float>()) {
            return {typeid(float), sizeof(float), 1, true};
        }
        if (handle.to_handle<const std::array<float, 2>>()) {
            return {typeid(std::array<float, 2>),
                    sizeof(std::array<float, 2>),
                    2,
                    true};
        }
        if (handle.to_handle<const std::array<float, 3>>()) {
            return {typeid(std::array<float, 3>),
                    sizeof(std::array<float, 3>),
                    3,
                    true};
        }
        if (handle.to_handle<const std::array<float, 4>>()) {
            return {typeid(std::array<float, 4>),
                    sizeof(std::array<float, 4>),
                    4,
                    true};
        }
        // Try double types.
        if (handle.to_handle<const double>()) {
            return {typeid(double), sizeof(double), 1, true};
        }
        if (handle.to_handle<const std::array<double, 2>>()) {
            return {typeid(std::array<double, 2>),
                    sizeof(std::array<double, 2>),
                    2,
                    true};
        }
        if (handle.to_handle<const std::array<double, 3>>()) {
            return {typeid(std::array<double, 3>),
                    sizeof(std::array<double, 3>),
                    3,
                    true};
        }
        if (handle.to_handle<const std::array<double, 4>>()) {
            return {typeid(std::array<double, 4>),
                    sizeof(std::array<double, 4>),
                    4,
                    true};
        }
        // Try integer types.
        if (handle.to_handle<const int>()) {
            return {typeid(int), sizeof(int), 1, false};
        }
        if (handle.to_handle<const int64_t>()) {
            return {typeid(int64_t), sizeof(int64_t), 1, false};
        }
        if (handle.to_handle<const std::array<int, 2>>()) {
            return {typeid(std::array<int, 2>),
                    sizeof(std::array<int, 2>),
                    2,
                    false};
        }
        if (handle.to_handle<const std::array<int, 3>>()) {
            return {typeid(std::array<int, 3>),
                    sizeof(std::array<int, 3>),
                    3,
                    false};
        }
        if (handle.to_handle<const std::array<int64_t, 2>>()) {
            return {typeid(std::array<int64_t, 2>),
                    sizeof(std::array<int64_t, 2>),
                    2,
                    false};
        }
        if (handle.to_handle<const std::array<int64_t, 3>>()) {
            return {typeid(std::array<int64_t, 3>),
                    sizeof(std::array<int64_t, 3>),
                    3,
                    false};
        }
        return {};
    }

    bool is_double_type(std::type_index tid) {
        return tid == typeid(double) || tid == typeid(std::array<double, 2>)
               || tid == typeid(std::array<double, 3>)
               || tid == typeid(std::array<double, 4>);
    }

    bool is_float_type(std::type_index tid) {
        return tid == typeid(float) || tid == typeid(std::array<float, 2>)
               || tid == typeid(std::array<float, 3>)
               || tid == typeid(std::array<float, 4>);
    }

    // ── CachedAttribute builders for double→float conversion ────────────
    //
    // Each function creates a StoredAttribute<TargetT> filled manually
    // from a typed source handle.  The result is owned by MeshData via
    // unique_ptr<StoredAttributeBase>.

    struct CacheResult {
        std::unique_ptr<qattr::StoredAttributeBase> owned;
        qattr::ConstTypeErasedAttributeHandle handle;
        uint8_t component_count = 0;
    };

    // Helper: build a StoredAttribute<TargetT> by applying a functor
    // to each element of a typed source handle.
    template <typename TargetT, typename SourceT, typename Functor>
    CacheResult make_cached(qattr::AttributeHandle<const SourceT> src_handle,
                            Functor fn,
                            uint8_t components) {
        auto result = std::make_unique<qattr::StoredAttribute<TargetT>>();
        const std::size_t n = src_handle.size();
        result->resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            (*result)[i] = fn(src_handle[i]);
        }

        CacheResult cr;
        cr.handle = qattr::ConstTypeErasedAttributeHandle(*result);
        cr.owned = std::move(result);
        cr.component_count = components;
        return cr;
    }

    // Build a float conversion cache for a vector-valued attribute.
    // Dispatches on the probed type_id.
    CacheResult
        build_float_cache_vector(qattr::ConstTypeErasedAttributeHandle source,
                                 std::type_index tid) {
        if (tid == typeid(std::array<double, 3>)) {
            auto h = source.to_handle<const std::array<double, 3>>();
            if (!h) return {};
            return make_cached<std::array<float, 3>>(
                *h,
                [](const std::array<double, 3> &v) -> std::array<float, 3> {
                    return {float(v[0]), float(v[1]), float(v[2])};
                },
                3);
        }
        if (tid == typeid(std::array<double, 2>)) {
            auto h = source.to_handle<const std::array<double, 2>>();
            if (!h) return {};
            return make_cached<std::array<float, 2>>(
                *h,
                [](const std::array<double, 2> &v) -> std::array<float, 2> {
                    return {float(v[0]), float(v[1])};
                },
                2);
        }
        if (tid == typeid(std::array<double, 4>)) {
            auto h = source.to_handle<const std::array<double, 4>>();
            if (!h) return {};
            return make_cached<std::array<float, 4>>(
                *h,
                [](const std::array<double, 4> &v) -> std::array<float, 4> {
                    return {float(v[0]), float(v[1]), float(v[2]), float(v[3])};
                },
                4);
        }
        if (tid == typeid(double)) {
            auto h = source.to_handle<const double>();
            if (!h) return {};
            return make_cached<float>(
                *h, [](const double &v) -> float { return float(v); }, 1);
        }
        return {};
    }

    // Build a float conversion cache for a scalar attribute, potentially
    // extracting a single component from a multi-component source.
    CacheResult
        build_float_cache_scalar(qattr::ConstTypeErasedAttributeHandle source,
                                 std::type_index tid,
                                 int component) {
        // Already float scalar — no conversion needed if component == -1.
        if (tid == typeid(float) && component < 0) {
            return {}; // caller should use source directly
        }

        // Double scalar → float.
        if (tid == typeid(double)) {
            auto h = source.to_handle<const double>();
            if (!h) return {};
            return make_cached<float>(
                *h, [](const double &v) -> float { return float(v); }, 1);
        }

        // Float array — extract component or magnitude.
        auto extract_float_array =
            [&]<std::size_t N>(
                qattr::AttributeHandle<const std::array<float, N>> h)
            -> CacheResult {
            if (component >= 0 && component < static_cast<int>(N)) {
                int c = component;
                return make_cached<float>(
                    h,
                    [c](const std::array<float, N> &v) -> float {
                        return v[static_cast<std::size_t>(c)];
                    },
                    1);
            }
            // Magnitude.
            return make_cached<float>(
                h,
                [](const std::array<float, N> &v) -> float {
                    float sum = 0.0f;
                    for (std::size_t i = 0; i < N; ++i) sum += v[i] * v[i];
                    return std::sqrt(sum);
                },
                1);
        };

        // Double array — extract component or magnitude, convert to float.
        auto extract_double_array =
            [&]<std::size_t N>(
                qattr::AttributeHandle<const std::array<double, N>> h)
            -> CacheResult {
            if (component >= 0 && component < static_cast<int>(N)) {
                int c = component;
                return make_cached<float>(
                    h,
                    [c](const std::array<double, N> &v) -> float {
                        return float(v[static_cast<std::size_t>(c)]);
                    },
                    1);
            }
            // Magnitude.
            return make_cached<float>(
                h,
                [](const std::array<double, N> &v) -> float {
                    double sum = 0.0;
                    for (std::size_t i = 0; i < N; ++i) sum += v[i] * v[i];
                    return float(std::sqrt(sum));
                },
                1);
        };

        // Integer types — convert to float.
        if (tid == typeid(int)) {
            auto h = source.to_handle<const int>();
            if (!h) return {};
            return make_cached<float>(
                *h, [](const int &v) -> float { return float(v); }, 1);
        }
        if (tid == typeid(int64_t)) {
            auto h = source.to_handle<const int64_t>();
            if (!h) return {};
            return make_cached<float>(
                *h, [](const int64_t &v) -> float { return float(v); }, 1);
        }

        // Dispatch array types.
        if (tid == typeid(std::array<float, 2>)) {
            auto h = source.to_handle<const std::array<float, 2>>();
            if (h) return extract_float_array.template operator()<2>(*h);
        }
        if (tid == typeid(std::array<float, 3>)) {
            auto h = source.to_handle<const std::array<float, 3>>();
            if (h) return extract_float_array.template operator()<3>(*h);
        }
        if (tid == typeid(std::array<float, 4>)) {
            auto h = source.to_handle<const std::array<float, 4>>();
            if (h) return extract_float_array.template operator()<4>(*h);
        }
        if (tid == typeid(std::array<double, 2>)) {
            auto h = source.to_handle<const std::array<double, 2>>();
            if (h) return extract_double_array.template operator()<2>(*h);
        }
        if (tid == typeid(std::array<double, 3>)) {
            auto h = source.to_handle<const std::array<double, 3>>();
            if (h) return extract_double_array.template operator()<3>(*h);
        }
        if (tid == typeid(std::array<double, 4>)) {
            auto h = source.to_handle<const std::array<double, 4>>();
            if (h) return extract_double_array.template operator()<4>(*h);
        }

        return {};
    }

    // Extract triangle indices from a Mesh<2>.
    void extract_tri_indices(const quiver::Mesh<2> &mesh,
                             std::vector<uint32_t> &out) {
        const auto &skel0 = mesh.skeleton<0>();
        quiver::attributes::IncidentFaceIndices<2, 0, 2> tri_verts(skel0);

        std::size_t n_tris = tri_verts.size();
        out.resize(n_tris * 3);
        for (std::size_t t = 0; t < n_tris; ++t) {
            auto [v0, v1, v2] = tri_verts.get_indices(t);
            out[t * 3 + 0] = static_cast<uint32_t>(v0);
            out[t * 3 + 1] = static_cast<uint32_t>(v1);
            out[t * 3 + 2] = static_cast<uint32_t>(v2);
        }
    }

    // Extract edge indices from a Mesh<2>.
    void extract_edge_indices(const quiver::Mesh<2> &mesh,
                              std::vector<uint32_t> &out) {
        const auto &skel1 = mesh.skeleton<1>();
        const auto &skel0 = mesh.skeleton<0>();
        quiver::attributes::IncidentFaceIndices<1, 0, 2> edge_verts(skel1,
                                                                    skel0);

        std::size_t n_edges = edge_verts.size();
        out.resize(n_edges * 2);
        for (std::size_t e = 0; e < n_edges; ++e) {
            auto [v0, v1] = edge_verts.get_indices(e);
            out[e * 2 + 0] = static_cast<uint32_t>(v0);
            out[e * 2 + 1] = static_cast<uint32_t>(v1);
        }
    }

    // Extract edge indices from a Mesh<1> (edge mesh — no triangle topology).
    void extract_edge_indices_1d(const quiver::Mesh<1> &mesh,
                                 std::vector<uint32_t> &out) {
        const auto &skel0 = mesh.skeleton<0>();
        quiver::attributes::IncidentFaceIndices<1, 0, 1> edge_verts(skel0);

        std::size_t n_edges = edge_verts.size();
        out.resize(n_edges * 2);
        for (std::size_t e = 0; e < n_edges; ++e) {
            auto [v0, v1] = edge_verts.get_indices(e);
            out[e * 2 + 0] = static_cast<uint32_t>(v0);
            out[e * 2 + 1] = static_cast<uint32_t>(v1);
        }
    }

    // Extract triangle indices from a Mesh<3> (tetrahedra → triangle faces).
    void extract_tri_indices_3d(const quiver::Mesh<3> &mesh,
                                std::vector<uint32_t> &out) {
        const auto &skel2 = mesh.skeleton<2>();
        const auto &skel0 = mesh.skeleton<0>();
        quiver::attributes::IncidentFaceIndices<2, 0, 3> tri_verts(skel2,
                                                                   skel0);

        std::size_t n_tris = tri_verts.size();
        out.resize(n_tris * 3);
        for (std::size_t t = 0; t < n_tris; ++t) {
            auto [v0, v1, v2] = tri_verts.get_indices(t);
            out[t * 3 + 0] = static_cast<uint32_t>(v0);
            out[t * 3 + 1] = static_cast<uint32_t>(v1);
            out[t * 3 + 2] = static_cast<uint32_t>(v2);
        }
    }

    // Extract edge indices from a Mesh<3> (tetrahedra → edges).
    void extract_edge_indices_3d(const quiver::Mesh<3> &mesh,
                                 std::vector<uint32_t> &out) {
        const auto &skel1 = mesh.skeleton<1>();
        const auto &skel0 = mesh.skeleton<0>();
        quiver::attributes::IncidentFaceIndices<1, 0, 3> edge_verts(skel1,
                                                                    skel0);

        std::size_t n_edges = edge_verts.size();
        out.resize(n_edges * 2);
        for (std::size_t e = 0; e < n_edges; ++e) {
            auto [v0, v1] = edge_verts.get_indices(e);
            out[e * 2 + 0] = static_cast<uint32_t>(v0);
            out[e * 2 + 1] = static_cast<uint32_t>(v1);
        }
    }

} // anonymous namespace

// ── MeshData ────────────────────────────────────────────────────────

MeshData::MeshData() = default;

void MeshData::set_mesh(std::shared_ptr<quiver::MeshBase> mesh) {
    _mesh = std::move(mesh);
    _position.clear();
    _normal.clear();
    _scalar.clear();
    _scalar_component = -1;
    _tri_indices.clear();
    _edge_indices.clear();
    _discovered.clear();

    if (!_mesh) {
        ++_version;
        return;
    }

    // Build all sub-skeletons so topology queries work.
    _mesh->build_all_skeletons();

    // Enumerate attributes.
    discover_attributes();

    // Auto-assign roles by convention name.
    for (const auto &da : _discovered) {
        if (da.name == "vertex_positions" && !_position.is_bound()) {
            assign_position(da.handle);
        } else if (da.name == "vertex_normals" && !_normal.is_bound()) {
            assign_normal(da.handle);
        }
    }

    // Extract topology indices.
    extract_topology_indices();

    ++_version;
}

// ── Attribute discovery ─────────────────────────────────────────────

void MeshData::discover_attributes() {
    _discovered.clear();
    if (!_mesh) return;

    auto entries = _mesh->attribute_entries();
    for (const auto &entry : entries) {
        if (!entry.attribute) continue;

        auto *stored = dynamic_cast<const qattr::StoredAttributeBase *>(
            entry.attribute.get());
        if (!stored) continue;

        // Create a type-erased handle to this attribute.
        auto handle = qattr::ConstTypeErasedAttributeHandle(*stored);

        // Probe the concrete type via dynamic_cast.
        auto probe = probe_type(handle);
        if (probe.component_count == 0) continue; // unrecognised type

        DiscoveredAttribute da;
        da.handle = handle;
        da.name = entry.name;
        da.dimension = entry.dim;
        da.type_id = probe.type_id;
        da.element_size = probe.element_size;
        da.count = stored->size();
        da.component_count = probe.component_count;
        da.is_floating_point = probe.is_floating_point;

        _discovered.push_back(std::move(da));
    }
}

void MeshData::rediscover_attributes() {
    if (!_mesh) return;

    discover_attributes();
    ++_version;
}

// ── Topology index extraction ───────────────────────────────────────

void MeshData::extract_topology_indices() {
    _tri_indices.clear();
    _edge_indices.clear();
    if (!_mesh) return;

    int8_t dim = _mesh->dimension();

    if (dim == 2) {
        auto &mesh2 = _mesh->as<2, true>();
        extract_tri_indices(mesh2, _tri_indices);
        extract_edge_indices(mesh2, _edge_indices);
    } else if (dim == 3) {
        auto &mesh3 = _mesh->as<3, true>();
        extract_tri_indices_3d(mesh3, _tri_indices);
        extract_edge_indices_3d(mesh3, _edge_indices);
    } else if (dim == 1) {
        auto &mesh1 = _mesh->as<1, true>();
        extract_edge_indices_1d(mesh1, _edge_indices);
    }
}

// ── Role assignment ─────────────────────────────────────────────────

void MeshData::assign_position(qattr::ConstTypeErasedAttributeHandle handle) {
    _position = build_vector_binding(handle);
    ++_version;
}

void MeshData::assign_normal(qattr::ConstTypeErasedAttributeHandle handle) {
    _normal = build_vector_binding(handle);
    ++_version;
}

void MeshData::assign_scalar(qattr::ConstTypeErasedAttributeHandle handle,
                             int component) {
    _scalar_component = component;
    _scalar = build_scalar_binding(handle, component);
    ++_version;
}

void MeshData::clear_position() {
    _position.clear();
    ++_version;
}

void MeshData::clear_normal() {
    _normal.clear();
    ++_version;
}

void MeshData::clear_scalar() {
    _scalar.clear();
    _scalar_component = -1;
    ++_version;
}

// ── Accessors ───────────────────────────────────────────────────────

std::span<const uint32_t> MeshData::triangle_indices() const {
    return _tri_indices;
}

std::span<const uint32_t> MeshData::edge_indices() const {
    return _edge_indices;
}

std::size_t MeshData::vertex_count() const {
    if (_position.is_bound()) return _position.size();
    return 0;
}

// ── Static helpers: build role bindings ──────────────────────────────

RoleBinding MeshData::build_vector_binding(
    qattr::ConstTypeErasedAttributeHandle handle) {
    RoleBinding binding;
    if (!handle.valid()) return binding;

    auto probe = probe_type(handle);
    if (probe.component_count == 0 || !probe.is_floating_point) {
        spdlog::warn(
            "MeshData: attribute type is not a supported floating-point vector "
            "type");
        return binding;
    }

    binding.source = handle;

    if (is_float_type(probe.type_id)) {
        // Already float — gpu_ready points directly at source.
        binding.gpu_ready = handle;
        binding.component_count = probe.component_count;
    } else if (is_double_type(probe.type_id)) {
        // Double → float conversion needed.
        auto cr = build_float_cache_vector(handle, probe.type_id);
        if (cr.owned) {
            binding.gpu_ready = cr.handle;
            binding.cached = std::move(cr.owned);
            binding.component_count = cr.component_count;
        } else {
            spdlog::warn(
                "MeshData: failed to build float cache for double attribute");
            return {};
        }
    }

    return binding;
}

RoleBinding
    MeshData::build_scalar_binding(qattr::ConstTypeErasedAttributeHandle handle,
                                   int component) {
    RoleBinding binding;
    if (!handle.valid()) return binding;

    auto probe = probe_type(handle);
    if (probe.component_count == 0) {
        spdlog::warn("MeshData: unrecognised attribute type for scalar role");
        return binding;
    }

    binding.source = handle;

    // Simple case: already a single float, no component extraction.
    if (probe.type_id == typeid(float) && component < 0) {
        binding.gpu_ready = handle;
        binding.component_count = 1;
        return binding;
    }

    // Need conversion or component extraction.
    auto cr = build_float_cache_scalar(handle, probe.type_id, component);
    if (cr.owned) {
        binding.gpu_ready = cr.handle;
        binding.cached = std::move(cr.owned);
        binding.component_count = 1;
    } else if (probe.type_id == typeid(float) && component < 0) {
        // build_float_cache_scalar returns empty for float with no
        // component extraction — use source directly.
        binding.gpu_ready = handle;
        binding.component_count = 1;
    } else {
        spdlog::warn(
            "MeshData: failed to build float cache for scalar attribute");
        return {};
    }

    return binding;
}

} // namespace balsa::scene_graph
