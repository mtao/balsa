#pragma once

#include <array>
#include <functional>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "balsa/scene_graph/render_layers.hpp"
#include "balsa/scene_graph/MeshAttribute.hpp"
#include "balsa/visualization/controls/Layout.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::scene_graph {

class BasicShading;

inline constexpr std::string_view position_role = "balsa.position";
inline constexpr std::string_view normal_role = "balsa.normal";
inline constexpr std::string_view scalar_role = "balsa.scalar";

struct MeshLayerId {
    uint64_t value = 0;
    constexpr auto operator==(const MeshLayerId &) const -> bool = default;
};

/// Geometry topology selected by a renderer layer.
enum class MeshPrimitive : uint8_t { Triangles, Edges, Vertices };

/// A semantic input a renderer reads from the geometry.
///
/// A layer binds each role to a raw attribute; how the values are
/// interpreted (e.g. reducing a vector to a scalar) is up to the renderer
/// and its shader.  Roles a layer does not bind explicitly fall back to
/// default_attribute, if any.
struct MeshRoleRequirement {
    std::string id;
    AttributeDomain domain = AttributeDomain::Vertex;
    uint8_t min_components = 1;
    uint8_t max_components = MeshAttribute::max_components;
    bool required = true;
    std::string default_attribute = {};

    [[nodiscard]] auto accepts(const MeshAttribute &attribute) const -> bool {
        return attribute.domain == domain
               && attribute.component_count() >= min_components
               && attribute.component_count() <= max_components;
    }
};

/// An explicit binding of one role.  A null attribute means the role was
/// explicitly unbound (it does not fall back to the default attribute).
struct MeshRoleBinding {
    MeshAttributeHandle attribute;
};

/// How a renderer reduces a multi-component attribute to one scalar.
/// Evaluated by the shader on the raw values.
struct ScalarReduction {
    enum class Kind : uint8_t { Component, Norm };
    Kind kind = Kind::Norm;
    /// Component index for Component, p (>= 1) for Norm.
    uint8_t value = 2;

    constexpr auto operator==(const ScalarReduction &) const -> bool = default;
};

/// [min, max] of attribute reduced as the shader reduces it, or nullopt
/// for an empty attribute.  A component the attribute lacks reads as 0.
[[nodiscard]] auto scalar_extents(const MeshAttribute &attribute,
                                  ScalarReduction reduction)
    -> std::optional<std::array<float, 2>>;

/// Colouring by a scalar, applied whenever scalar_role is bound (the
/// binding is the switch): the attribute, reduced by
/// `reduction` in the shader, normalized over the range and looked up in
/// `colormap` (a name from the colormap_shaders subproject, see
/// visualization::shaders::colormap_names()).
///
/// With fit_to_data (the default) the range is the reduced data's extents
/// and follows the data; setting either end clears it and the range is
/// [min, max].
struct ScalarColormap {
    ScalarReduction reduction = {};
    std::string colormap = "MATLAB_jet";
    bool fit_to_data = true;
    float min = 0.0f;
    float max = 1.0f;

    auto operator==(const ScalarColormap &) const -> bool = default;
};

/// The backend-neutral information required for one built-in mesh draw.
///
/// This is intentionally a request, not a stored mesh-wide state: each
/// renderer instance supplies one independently configured draw.
struct MeshDrawRequest {
    MeshPrimitive primitive = MeshPrimitive::Triangles;
    visualization::vulkan::MeshRenderState state;
    /// Colormap applied to the scalar input; empty, or no scalar bound,
    /// draws in uniform colour.
    std::string colormap = {};
};

/// Renderer-owned material payload. Backends translate this into their own
/// descriptor/UBO layout; it deliberately contains no Vulkan types.
struct MeshMaterial {
    Vec4f uniform_color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4f layer_color{1.0f, 1.0f, 1.0f, 1.0f};
    float primitive_size = 1.0f;
    ScalarReduction scalar_reduction = {};
    float scalar_min = 0.0f;
    float scalar_max = 1.0f;
};

/// A live view of the layer a renderer draws for: what its roles resolve
/// to in the current geometry.  A default-constructed context is detached
/// and resolves nothing.
class MeshLayerContext {
  public:
    MeshLayerContext() = default;
    MeshLayerContext(const BasicShading &shading, MeshLayerId layer)
        : m_shading(&shading), m_layer(layer) {}

    /// The attribute role resolves to, or an invalid handle.
    [[nodiscard]] auto attribute(std::string_view role) const -> MeshAttributeHandle;
    /// Changes whenever the geometry's data changes.
    [[nodiscard]] auto geometry_version() const -> uint64_t;

  private:
    const BasicShading *m_shading = nullptr;
    MeshLayerId m_layer;
};

class MeshRenderer {
  public:
    virtual ~MeshRenderer() = default;

    [[nodiscard]] virtual auto clone() const -> std::unique_ptr<MeshRenderer> = 0;
    [[nodiscard]] virtual auto id() const -> std::string_view = 0;
    [[nodiscard]] virtual auto required_roles() const
        -> std::span<const MeshRoleRequirement> = 0;
    /// The requirement for role, or nullptr if this renderer does not read it.
    [[nodiscard]] auto requirement(std::string_view role) const
        -> const MeshRoleRequirement *;
    [[nodiscard]] virtual auto draw_request() const -> MeshDrawRequest = 0;
    /// Material for drawing layer.
    virtual auto write_material(MeshMaterial &material,
                                const MeshLayerContext &layer) const -> void = 0;
    [[nodiscard]] virtual auto line_width() const -> float { return 1.0f; }
    /// Controls for this renderer's settings; layer stays valid while the
    /// controls are shown.
    [[nodiscard]] virtual auto build_controls(const MeshLayerContext &layer)
        -> visualization::controls::Layout = 0;
};

struct MeshRendererDefinition {
    std::string id;
    std::string label;
    std::function<std::unique_ptr<MeshRenderer>()> create;
};

auto register_mesh_renderer(MeshRendererDefinition definition) -> void;
[[nodiscard]] auto registered_mesh_renderers()
    -> std::vector<const MeshRendererDefinition *>;
[[nodiscard]] auto create_mesh_renderer(std::string_view id)
    -> std::unique_ptr<MeshRenderer>;

struct MeshRenderLayer {
    MeshLayerId id;
    std::string name;
    bool enabled = true;
    std::unique_ptr<MeshRenderer> renderer;
    std::unordered_map<std::string, MeshRoleBinding> bindings;

    MeshRenderLayer() = default;
    MeshRenderLayer(std::string name, std::unique_ptr<MeshRenderer> renderer)
        : name(std::move(name)), renderer(std::move(renderer)) {}
    MeshRenderLayer(const MeshRenderLayer &other);
    auto operator=(const MeshRenderLayer &other) -> MeshRenderLayer &;
    MeshRenderLayer(MeshRenderLayer &&) noexcept = default;
    auto operator=(MeshRenderLayer &&) noexcept -> MeshRenderLayer & = default;

    /// Explicit binding of role; nullptr if the role uses its default.
    [[nodiscard]] auto binding(std::string_view role) const -> const MeshRoleBinding * {
        const auto it = bindings.find(std::string(role));
        return it == bindings.end() ? nullptr : &it->second;
    }
};

class PhongSurfaceRenderer final : public MeshRenderer {
  public:
    [[nodiscard]] auto clone() const -> std::unique_ptr<MeshRenderer> override;
    [[nodiscard]] auto id() const -> std::string_view override;
    [[nodiscard]] auto required_roles() const
        -> std::span<const MeshRoleRequirement> override;
    [[nodiscard]] auto draw_request() const -> MeshDrawRequest override;
    auto write_material(MeshMaterial &material,
                        const MeshLayerContext &layer) const -> void override;
    [[nodiscard]] auto build_controls(const MeshLayerContext &layer)
        -> visualization::controls::Layout override;

    visualization::vulkan::MeshRenderState state;
    /// How the attribute bound to scalar_role colours the surface.
    ScalarColormap scalar_colormap = {};

    /// Extents of layer's scalar under scalar_colormap.reduction; cached
    /// until the binding, the data or the reduction changes.
    [[nodiscard]] auto scalar_data_extents(const MeshLayerContext &layer) const
        -> std::optional<std::array<float, 2>>;
    /// The colormap's effective range for layer.
    [[nodiscard]] auto scalar_range(const MeshLayerContext &layer) const
        -> std::array<float, 2>;

  private:
    struct ExtentsCache {
        MeshAttributeHandle attribute;
        uint64_t geometry_version = 0;
        ScalarReduction reduction;
        std::optional<std::array<float, 2>> extents;
    };
    mutable ExtentsCache m_extents;
};

class UnlitWireframeRenderer final : public MeshRenderer {
  public:
    [[nodiscard]] auto clone() const -> std::unique_ptr<MeshRenderer> override;
    [[nodiscard]] auto id() const -> std::string_view override;
    [[nodiscard]] auto required_roles() const
        -> std::span<const MeshRoleRequirement> override;
    [[nodiscard]] auto draw_request() const -> MeshDrawRequest override;
    auto write_material(MeshMaterial &material,
                        const MeshLayerContext &layer) const -> void override;
    [[nodiscard]] auto line_width() const -> float override { return width; }
    [[nodiscard]] auto build_controls(const MeshLayerContext &layer)
        -> visualization::controls::Layout override;

    Vec4f color{0.0f, 0.0f, 0.0f, 1.0f};
    float width = 1.5f;
};

class UnlitPointsRenderer final : public MeshRenderer {
  public:
    [[nodiscard]] auto clone() const -> std::unique_ptr<MeshRenderer> override;
    [[nodiscard]] auto id() const -> std::string_view override;
    [[nodiscard]] auto required_roles() const
        -> std::span<const MeshRoleRequirement> override;
    [[nodiscard]] auto draw_request() const -> MeshDrawRequest override;
    auto write_material(MeshMaterial &material,
                        const MeshLayerContext &layer) const -> void override;
    [[nodiscard]] auto build_controls(const MeshLayerContext &layer)
        -> visualization::controls::Layout override;

    Vec4f color{1.0f, 0.0f, 0.0f, 1.0f};
    float size = 3.0f;
};

auto register_builtin_mesh_renderers() -> void;
[[nodiscard]] auto make_default_mesh_render_layers() -> std::vector<MeshRenderLayer>;

} // namespace balsa::scene_graph
