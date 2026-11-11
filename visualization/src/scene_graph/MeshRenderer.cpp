#include "balsa/scene_graph/MeshRenderer.hpp"
#include "balsa/scene_graph/AttributeSemantic.hpp"
#include "balsa/scene_graph/MeshGeometry.hpp"

#include <algorithm>
#include <format>

#include <zipper/MatrixBase.hxx>
#include <zipper/utils/max_coeff.hpp>
#include <zipper/utils/min_coeff.hpp>

#include "balsa/visualization/colormap_list.hpp"

namespace balsa::scene_graph {

namespace {

auto definitions() -> std::vector<MeshRendererDefinition> & {
    static std::vector<MeshRendererDefinition> value;
    return value;
}

auto bool_property(std::string label, bool &field) -> visualization::controls::Property {
    return {.label = std::move(label),
            .widget = visualization::controls::BoolInput{
                .get = [&field] { return field; },
                .set = [&field](const bool &value) { field = value; }}};
}

auto color_property(std::string label, Vec4f &field)
    -> visualization::controls::Property {
    return {.label = std::move(label),
            .widget = visualization::controls::ColorInput{
                .get = [&field] { return field; },
                .set = [&field](const Vec4f &value) { field = value; }}};
}

auto float_property(std::string label, float &field, float min, float max,
                    float step, bool slider = false)
    -> visualization::controls::Property {
    return {.label = std::move(label),
            .widget = visualization::controls::FloatInput{
                .get = [&field] { return field; },
                .set = [&field](const float &value) { field = value; },
                .min = min,
                .max = max,
                .step = step,
                .use_slider = slider}};
}

// "balsa.unlit_lines" / "balsa.unlit_points"'s std140 material block.
struct UnlitBlock {
    Vec4f color;
    Vec4f size;   // x = point size
};

auto position_requirement() -> MeshRoleRequirement {
    return {.role = roles::position(),
            .min_components = 1,
            .max_components = 4,
            .semantic = semantics::position()};
}

auto scalar_reduction_options() -> std::vector<std::string> {
    return {"Norm L1", "Norm L2", "Norm L3", "X", "Y", "Z", "W"};
}

auto scalar_reduction_index(const ScalarReduction &reduction) -> int {
    return reduction.kind == ScalarReduction::Kind::Norm
               ? std::clamp<int>(reduction.value, 1, 3) - 1
               : 3 + reduction.value;
}

auto scalar_reduction_from_index(int index) -> ScalarReduction {
    return index < 3 ? ScalarReduction{.kind = ScalarReduction::Kind::Norm,
                                       .value = static_cast<uint8_t>(index + 1)}
                     : ScalarReduction{.kind = ScalarReduction::Kind::Component,
                                       .value = static_cast<uint8_t>(index - 3)};
}

auto extents_text(const std::optional<std::array<float, 2>> &extents) -> std::string {
    return extents ? std::format("Data range: [{:g}, {:g}]", (*extents)[0], (*extents)[1])
                   : std::string("No scalar attribute bound");
}

} // namespace

auto scalar_extents(const MeshAttribute &attribute, ScalarReduction reduction)
    -> std::optional<std::array<float, 2>> {
    if (attribute.count() == 0) return std::nullopt;
    return attribute.visit([&](const auto &values) -> std::array<float, 2> {
        const auto data = ::zipper::MatrixBase(values).template cast<double>();
        auto extents_of = [](const auto &scalars) {
            return std::array{static_cast<float>(::zipper::utils::minCoeff(scalars)),
                              static_cast<float>(::zipper::utils::maxCoeff(scalars))};
        };
        if (reduction.kind == ScalarReduction::Kind::Component) {
            if (reduction.value >= values.extent(0)) return {0.0f, 0.0f};
            return extents_of(data.row(reduction.value));
        }
        switch (reduction.value) {
        case 1: return extents_of(::zipper::as_vector(data.colwise().template norm<1>()));
        case 3: return extents_of(::zipper::as_vector(data.colwise().template norm<3>()));
        default: return extents_of(::zipper::as_vector(data.colwise().template norm<2>()));
        }
    });
}

auto MeshRenderer::requirement(MeshRole role) const
    -> const MeshRoleRequirement * {
    const auto roles = required_roles();
    const auto it = std::ranges::find(roles, role, &MeshRoleRequirement::role);
    return it == roles.end() ? nullptr : &*it;
}

auto MeshRenderer::fit_to(const MeshLayerContext &layer) -> bool {
    const auto *geometry = layer.geometry();
    if (geometry == nullptr) return false;
    switch (draw_request().primitive) {
    case MeshPrimitive::Triangles:
        if (!geometry->has_triangle_indices()) return false;
        break;
    case MeshPrimitive::Edges:
        if (!geometry->has_edge_indices()) return false;
        break;
    case MeshPrimitive::Vertices: break;
    }
    return std::ranges::all_of(required_roles(), [&](const MeshRoleRequirement &r) {
        return !r.required || layer.attribute(r.role).valid();
    });
}

MeshRenderLayer::MeshRenderLayer(const MeshRenderLayer &other)
    : id(other.id), name(other.name), enabled(other.enabled), view(other.view),
      renderer(other.renderer ? other.renderer->clone() : nullptr),
      bindings(other.bindings) {}

auto MeshRenderLayer::operator=(const MeshRenderLayer &other) -> MeshRenderLayer & {
    if (this == &other) return *this;
    id = other.id;
    name = other.name;
    enabled = other.enabled;
    view = other.view;
    renderer = other.renderer ? other.renderer->clone() : nullptr;
    bindings = other.bindings;
    return *this;
}

auto PhongSurfaceRenderer::clone() const -> std::unique_ptr<MeshRenderer> {
    return std::make_unique<PhongSurfaceRenderer>(*this);
}
auto PhongSurfaceRenderer::id() const -> std::string_view {
    return "balsa.phong_surface";
}
auto PhongSurfaceRenderer::required_roles() const
    -> std::span<const MeshRoleRequirement> {
    static const std::array requirements{
        position_requirement(),
        MeshRoleRequirement{.role = roles::normal(),
                            .min_components = 2,
                            .max_components = 4,
                            .required = false,
                            .semantic = semantics::normal()},
        // Up to 4 raw components; scalar_colormap.reduction picks the scalar.
        MeshRoleRequirement{.role = roles::scalar(),
                            .min_components = 1,
                            .max_components = 4,
                            .required = false},
    };
    return requirements;
}
auto PhongSurfaceRenderer::fit_to(const MeshLayerContext &layer) -> bool {
    state.constrain(layer.attribute(roles::normal()).valid());
    return MeshRenderer::fit_to(layer);
}
auto PhongSurfaceRenderer::draw_request() const -> MeshDrawRequest {
    using namespace visualization::vulkan;
    MeshDrawRequest request{.program = visualization::shaders::programs::phong(),
                            .primitive = MeshPrimitive::Triangles,
                            .cull_mode = state.cull_mode};
    auto &defines = request.variant.defines;
    if (state.normal_source == NormalSource::None) {
        defines.emplace_back("SHADING_UNLIT");
    } else {
        switch (state.shading) {
        case ShadingModel::Flat: defines.emplace_back("SHADING_FLAT"); break;
        case ShadingModel::Gouraud: defines.emplace_back("SHADING_GOURAUD"); break;
        case ShadingModel::Phong: defines.emplace_back("SHADING_PHONG"); break;
        }
    }
    if (!scalar_colormap.colormap.empty()) {
        defines.emplace_back("COLORMAP");
        request.variant.includes.emplace_back("colormap.glsl",
                                              "colormaps/" + scalar_colormap.colormap);
    }
    return request;
}
auto PhongSurfaceRenderer::scalar_data_extents(const MeshLayerContext &layer) const
    -> std::optional<std::array<float, 2>> {
    const auto attribute = layer.attribute(roles::scalar());
    if (!attribute) return std::nullopt;
    const auto version = layer.geometry_version();
    if (!(m_extents.attribute == attribute && m_extents.geometry_version == version
          && m_extents.reduction == scalar_colormap.reduction)) {
        m_extents = {.attribute = attribute,
                     .geometry_version = version,
                     .reduction = scalar_colormap.reduction,
                     .extents = scalar_extents(*attribute, scalar_colormap.reduction)};
    }
    return m_extents.extents;
}
auto PhongSurfaceRenderer::scalar_range(const MeshLayerContext &layer) const
    -> std::array<float, 2> {
    if (scalar_colormap.fit_to_data) {
        if (const auto extents = scalar_data_extents(layer)) return *extents;
    }
    return {scalar_colormap.min, scalar_colormap.max};
}
auto PhongSurfaceRenderer::write_material(MeshMaterial &material,
                                          const MeshLayerContext &layer) const -> void {
    // "balsa.phong"'s std140 material block.
    struct Block {
        Vec4f color;
        Vec4f response;       // ambient, diffuse, specular, shininess
        Vec4f scalar_range;   // min, max, two-sided
        Vec4f scalar_select;  // mode, component or p, component count
    };
    const auto [min, max] = scalar_range(layer);
    const auto scalar = layer.attribute(roles::scalar());
    const auto &reduction = scalar_colormap.reduction;
    const auto &m = state.material;
    material.set_uniform(Block{
        .color = state.uniform_color,
        .response = {m.ambient_strength, m.diffuse_strength, m.specular_strength, m.shininess},
        .scalar_range = {min, max, state.two_sided ? 1.0f : 0.0f, 0.0f},
        .scalar_select = {reduction.kind == ScalarReduction::Kind::Norm ? 1.0f : 0.0f,
                          static_cast<float>(reduction.value),
                          static_cast<float>(scalar ? scalar->component_count() : 0),
                          static_cast<float>(m_debug_view.channel())}});
}
auto PhongSurfaceRenderer::build_controls(const MeshLayerContext &layer)
    -> visualization::controls::Layout {
    using namespace visualization::controls;
    Layout layout;
    auto &scalar = scalar_colormap;
    auto uniform = color_property("Color", state.uniform_color);
    // Bound scalars replace the uniform colour.
    auto scalar_bound = [layer] { return layer.attribute(roles::scalar()).valid(); };
    uniform.visible_when = [scalar_bound] { return !scalar_bound(); };
    layout.add_property(std::move(uniform));
    layout.add_property(float_property("Ambient", state.material.ambient_strength,
                                      0.0f, 1.0f, 0.01f, true));
    layout.add_property(float_property("Diffuse", state.material.diffuse_strength,
                                      0.0f, 2.0f, 0.01f, true));
    layout.add_property(float_property("Specular", state.material.specular_strength,
                                      0.0f, 2.0f, 0.01f, true));
    layout.add_property(float_property("Shininess", state.material.shininess,
                                      1.0f, 256.0f, 1.0f, true));
    layout.add_property(bool_property("Two-sided Lighting", state.two_sided));
    layout.add_property(debug_view_property(m_debug_view));

    auto &colormap = layout.add_column("Scalar Colormap");
    colormap.set_visible_when(scalar_bound);
    colormap.add_property(
        {.label = "Scalar",
         .tooltip = "Reduction of the attribute bound to the scalar role",
         .widget = ChoiceInput{
             .options = scalar_reduction_options,
             .get_index = [&scalar] { return scalar_reduction_index(scalar.reduction); },
             .set_index =
                 [&scalar](const int &i) {
                     scalar.reduction = scalar_reduction_from_index(i);
                 }}});
    colormap.add_property(
        {.label = "Colormap",
         .widget = ChoiceInput{
             .options =
                 [] {
                     const auto names = visualization::shaders::colormap_names();
                     return std::vector<std::string>(names.begin(), names.end());
                 },
             .get_index =
                 [&scalar] { return visualization::find_colormap_index(scalar.colormap); },
             .set_index =
                 [&scalar](const int &i) {
                     const auto names = visualization::shaders::colormap_names();
                     if (i >= 0 && static_cast<std::size_t>(i) < names.size()) {
                         scalar.colormap = std::string(names[static_cast<std::size_t>(i)]);
                     }
                 }}});
    // The range follows the data until either end is edited.
    auto data_extents = [this, layer] { return extents_text(scalar_data_extents(layer)); };
    auto &range = colormap.add_row("Range");
    auto range_end = [&](std::string label, std::size_t end) {
        Property property{
            .label = std::move(label),
            .widget = FloatInput{
                .get = [this, layer, end] { return scalar_range(layer)[end]; },
                .set =
                    [this, layer, end](const float &value) {
                        auto bounds = scalar_range(layer);
                        bounds[end] = value;
                        scalar_colormap.fit_to_data = false;
                        scalar_colormap.min = bounds[0];
                        scalar_colormap.max = bounds[1];
                    },
                .min = -1e6f,
                .max = 1e6f,
                .step = 0.1f}};
        property.live_tooltip = data_extents;
        range.add_property(std::move(property));
    };
    range_end("Min", 0);
    range_end("Max", 1);
    Property fit{.label = "Fit to Data",
                 .widget = Button{.on_click = [&scalar] { scalar.fit_to_data = true; }}};
    fit.live_tooltip = data_extents;
    range.add_property(std::move(fit));
    return layout;
}

auto UnlitWireframeRenderer::clone() const -> std::unique_ptr<MeshRenderer> {
    return std::make_unique<UnlitWireframeRenderer>(*this);
}
auto UnlitWireframeRenderer::id() const -> std::string_view {
    return "balsa.unlit_wireframe";
}
auto UnlitWireframeRenderer::required_roles() const
    -> std::span<const MeshRoleRequirement> {
    static const std::array requirements{position_requirement()};
    return requirements;
}
auto UnlitWireframeRenderer::draw_request() const -> MeshDrawRequest {
    return {.program = visualization::shaders::programs::unlit_lines(),
            .primitive = MeshPrimitive::Edges};
}
auto UnlitWireframeRenderer::write_material(MeshMaterial &material,
                                            const MeshLayerContext &) const -> void {
    material.set_uniform(UnlitBlock{.color = color, .size = {width, 0.0f, 0.0f, 0.0f}});
}
auto UnlitWireframeRenderer::build_controls(const MeshLayerContext &)
    -> visualization::controls::Layout {
    using namespace visualization::controls;
    Layout layout;
    layout.add_property(color_property("Color", color));
    layout.add_property(float_property("Width", width, 0.5f, 5.0f, 0.1f, true));
    return layout;
}

auto UnlitPointsRenderer::clone() const -> std::unique_ptr<MeshRenderer> {
    return std::make_unique<UnlitPointsRenderer>(*this);
}
auto UnlitPointsRenderer::id() const -> std::string_view {
    return "balsa.unlit_points";
}
auto UnlitPointsRenderer::required_roles() const
    -> std::span<const MeshRoleRequirement> {
    static const std::array requirements{position_requirement()};
    return requirements;
}
auto UnlitPointsRenderer::draw_request() const -> MeshDrawRequest {
    return {.program = visualization::shaders::programs::unlit_points(),
            .primitive = MeshPrimitive::Vertices};
}
auto UnlitPointsRenderer::write_material(MeshMaterial &material,
                                         const MeshLayerContext &) const -> void {
    material.set_uniform(UnlitBlock{.color = color, .size = {size, 0.0f, 0.0f, 0.0f}});
}
auto UnlitPointsRenderer::build_controls(const MeshLayerContext &)
    -> visualization::controls::Layout {
    using namespace visualization::controls;
    Layout layout;
    layout.add_property(color_property("Color", color));
    layout.add_property(float_property("Size", size, 0.1f, 20.0f, 0.1f, true));
    return layout;
}

auto register_mesh_renderer(MeshRendererDefinition definition) -> void {
    auto &items = definitions();
    const auto it = std::ranges::find(items, definition.id, &MeshRendererDefinition::id);
    if (it == items.end()) items.push_back(std::move(definition));
    else *it = std::move(definition);
}

auto registered_mesh_renderers() -> std::vector<const MeshRendererDefinition *> {
    std::vector<const MeshRendererDefinition *> result;
    result.reserve(definitions().size());
    for (const auto &definition : definitions()) result.push_back(&definition);
    return result;
}

auto create_mesh_renderer(std::string_view id) -> std::unique_ptr<MeshRenderer> {
    const auto it = std::ranges::find(definitions(), id, &MeshRendererDefinition::id);
    return it == definitions().end() ? nullptr : it->create();
}

auto GltfMetallicRoughnessRenderer::clone() const
    -> std::unique_ptr<MeshRenderer> {
    return std::make_unique<GltfMetallicRoughnessRenderer>(*this);
}
auto GltfMetallicRoughnessRenderer::id() const -> std::string_view {
    return "balsa.gltf_metallic_roughness_surface";
}
auto GltfMetallicRoughnessRenderer::required_roles() const
    -> std::span<const MeshRoleRequirement> {
    static const std::array requirements{
        position_requirement(),
        // Without normals the shader uses face normals.
        MeshRoleRequirement{.role = roles::normal(),
                            .min_components = 3,
                            .max_components = 3,
                            .required = false,
                            .semantic = semantics::normal()},
        // xyz and the bitangent sign; without tangents the shader derives
        // them from texture coordinates.
        MeshRoleRequirement{.role = roles::tangent(),
                            .min_components = 4,
                            .max_components = 4,
                            .required = false,
                            .semantic = semantics::tangent()},
        MeshRoleRequirement{.role = roles::texture_coordinates(0),
                            .min_components = 2,
                            .max_components = 2,
                            .required = false,
                            .semantic = semantics::texture_coordinates(),
                            .set = 0},
        MeshRoleRequirement{.role = roles::texture_coordinates(1),
                            .min_components = 2,
                            .max_components = 2,
                            .required = false,
                            .semantic = semantics::texture_coordinates(),
                            .set = 1},
        // Multiplies the base colour.
        MeshRoleRequirement{.role = roles::color(),
                            .min_components = 3,
                            .max_components = 4,
                            .required = false,
                            .semantic = semantics::color()},
    };
    return requirements;
}
auto GltfMetallicRoughnessRenderer::draw_request() const -> MeshDrawRequest {
    using visualization::vulkan::CullMode;
    return {
        .program = visualization::shaders::programs::gltf_metallic_roughness(),
        .primitive = MeshPrimitive::Triangles,
        .cull_mode = material->double_sided ? CullMode::None : CullMode::Back,
        .depth_write = material->alpha_mode != AlphaMode::Blend};
}
auto GltfMetallicRoughnessRenderer::write_material(
    MeshMaterial &out,
    const MeshLayerContext &) const -> void {
    // "balsa.gltf_metallic_roughness"'s std140 material block.
    struct Block {
        Vec4f base_color;
        Vec4f emissive;                    // rgb, alpha cutoff
        Vec4f factors;                     // metallic, roughness, normal scale,
                                           // occlusion strength
        Vec4f texture_coordinate_sets;     // base colour, metallic-roughness,
                                           // normal, occlusion
        Vec4f options;                     // emissive set, alpha mode, double sided,
                                           // debug channel
    };
    const auto &m = *material;
    const auto set = [](const std::optional<MaterialTexture> &t) {
        return t ? static_cast<float>(t->set) : 0.0f;
    };
    out.set_uniform(Block{
        .base_color = m.base_color,
        .emissive = {m.emissive(0), m.emissive(1), m.emissive(2), m.alpha_cutoff},
        .factors = {m.metallic, m.roughness, m.normal_scale, m.occlusion_strength},
        .texture_coordinate_sets = {set(m.base_color_texture), set(m.metallic_roughness_texture),
                                    set(m.normal_texture), set(m.occlusion_texture)},
        .options = {set(m.emissive_texture), static_cast<float>(m.alpha_mode),
                    m.double_sided ? 1.0f : 0.0f, static_cast<float>(m_debug_view.channel())}});
    // In the program's texture slot order.
    out.textures = {m.base_color_texture, m.metallic_roughness_texture, m.normal_texture,
                    m.occlusion_texture, m.emissive_texture};
}
auto GltfMetallicRoughnessRenderer::build_controls(const MeshLayerContext &)
    -> visualization::controls::Layout {
    using namespace visualization::controls;
    Layout layout;
    auto &m = *material;
    layout.add_property(color_property("Base Color", m.base_color));
    layout.add_property(float_property("Metallic", m.metallic, 0.0f, 1.0f, 0.01f, true));
    layout.add_property(float_property("Roughness", m.roughness, 0.0f, 1.0f, 0.01f, true));
    layout.add_property(
        {.label = "Alpha Mode",
         .widget = ChoiceInput{
             .options = [] { return std::vector<std::string>{"Opaque", "Mask", "Blend"}; },
             .get_index = [&m] { return static_cast<int>(m.alpha_mode); },
             .set_index = [&m](const int &i) { m.alpha_mode = static_cast<AlphaMode>(i); }}});
    auto cutoff = float_property("Alpha Cutoff", m.alpha_cutoff, 0.0f, 1.0f, 0.01f, true);
    cutoff.visible_when = [&m] { return m.alpha_mode == AlphaMode::Mask; };
    layout.add_property(std::move(cutoff));
    layout.add_property(bool_property("Double Sided", m.double_sided));
    layout.add_property(debug_view_property(m_debug_view));
    return layout;
}

auto GltfMetallicRoughnessRenderer::debug_channels() -> std::span<const std::string_view> {
    // In gltf_metallic_roughness.frag's DEBUG_* order.
    static constexpr std::array<std::string_view, 9> names{
        "Base Color", "Alpha",          "Metallic", "Roughness", "Occlusion",
        "Emissive",   "Shading Normal", "Tangent",  "Bitangent"};
    return names;
}

auto PhongSurfaceRenderer::debug_channels() -> std::span<const std::string_view> {
    // In phong.frag's DEBUG_* order.
    static constexpr std::array<std::string_view, 3> names{"Diffuse", "Specular", "Scalar"};
    return names;
}

auto AttributeColorRenderer::clone() const -> std::unique_ptr<MeshRenderer> {
    return std::make_unique<AttributeColorRenderer>(*this);
}
auto AttributeColorRenderer::id() const -> std::string_view { return "balsa.attribute_color"; }
auto AttributeColorRenderer::required_roles() const -> std::span<const MeshRoleRequirement> {
    static const std::array requirements{
        position_requirement(),
        // Normals by default; without an attribute the shader shows face
        // normals.
        MeshRoleRequirement{.role = roles::visualized(),
                            .min_components = 1,
                            .max_components = 4,
                            .required = false,
                            .semantic = semantics::normal()},
    };
    return requirements;
}
auto AttributeColorRenderer::draw_request() const -> MeshDrawRequest {
    return {.program = visualization::shaders::programs::attribute_color(),
            .primitive = MeshPrimitive::Triangles};
}
auto AttributeColorRenderer::component_ranges(const MeshLayerContext &layer) const
    -> std::array<std::array<float, 2>, 4> {
    std::array<std::array<float, 2>, 4> ranges;
    switch (range) {
    case AttributeColorRange::Signed: ranges.fill({-1.0f, 1.0f}); return ranges;
    case AttributeColorRange::Unit:
    case AttributeColorRange::Fractional: ranges.fill({0.0f, 1.0f}); return ranges;
    case AttributeColorRange::FitToData: break;
    }
    const auto attribute = layer.attribute(roles::visualized());
    const auto version = layer.geometry_version();
    if (!(m_extents.attribute == attribute && m_extents.geometry_version == version)) {
        m_extents = {.attribute = attribute, .geometry_version = version};
        m_extents.extents.fill({0.0f, 1.0f});
        const auto count = attribute ? std::min<std::size_t>(attribute->component_count(), 4) : 0;
        for (std::size_t c = 0; c < count; ++c) {
            const ScalarReduction component{.kind = ScalarReduction::Kind::Component,
                                            .value = static_cast<uint8_t>(c)};
            if (const auto extents = scalar_extents(*attribute, component)) {
                m_extents.extents[c] = *extents;
            }
        }
    }
    return m_extents.extents;
}
auto AttributeColorRenderer::write_material(MeshMaterial &material,
                                            const MeshLayerContext &layer) const -> void {
    // "balsa.attribute_color"'s std140 material block.
    struct Block {
        Vec4f min;
        Vec4f max;
        Vec4f options;   // component count, fractional
    };
    const auto attribute = layer.attribute(roles::visualized());
    const auto ranges = component_ranges(layer);
    Block block{.options = {static_cast<float>(attribute ? attribute->component_count() : 0),
                            range == AttributeColorRange::Fractional ? 1.0f : 0.0f, 0.0f, 0.0f}};
    for (std::size_t c = 0; c < ranges.size(); ++c) {
        block.min(c) = ranges[c][0];
        block.max(c) = ranges[c][1];
    }
    if (!attribute) {
        // Face normals are directions whatever the range.
        block.min = Vec4f{-1.0f, -1.0f, -1.0f, -1.0f};
        block.max = Vec4f{1.0f, 1.0f, 1.0f, 1.0f};
        block.options(1) = 0.0f;
    }
    material.set_uniform(block);
}
auto AttributeColorRenderer::build_controls(const MeshLayerContext &layer)
    -> visualization::controls::Layout {
    using namespace visualization::controls;
    Layout layout;
    Property choice{
        .label = "Range",
        .tooltip = "Values shown as 0 to 1: [-1, 1] for directions, [0, 1] for colours, "
                   "the data's extents per component, or repeating (fract)",
        .widget = ChoiceInput{
            .options =
                [] {
                    return std::vector<std::string>{"Signed [-1, 1]", "Unit [0, 1]",
                                                    "Fit to Data", "Fractional"};
                },
            .get_index = [this] { return static_cast<int>(range); },
            .set_index =
                [this](const int &i) {
                    if (i >= 0 && i <= static_cast<int>(AttributeColorRange::Fractional)) {
                        range = static_cast<AttributeColorRange>(i);
                    }
                }}};
    choice.live_tooltip = [this, layer] {
        const auto attribute = layer.attribute(roles::visualized());
        if (!attribute) return std::string("Face normals");
        const auto ranges = component_ranges(layer);
        std::string text;
        for (std::size_t c = 0; c < std::min<std::size_t>(attribute->component_count(), 4); ++c) {
            text += std::format("[{:.3g}, {:.3g}] ", ranges[c][0], ranges[c][1]);
        }
        return text;
    };
    layout.add_property(std::move(choice));
    return layout;
}

auto register_builtin_mesh_renderers() -> void {
    static const bool once = [] {
        register_mesh_renderer({"balsa.phong_surface", "Phong Surface", [] {
                                    return std::make_unique<PhongSurfaceRenderer>();
                                }});
        register_mesh_renderer(
            {"balsa.gltf_metallic_roughness_surface",
             "glTF Metallic-Roughness",
             [] { return std::make_unique<GltfMetallicRoughnessRenderer>(); }});
        register_mesh_renderer({"balsa.attribute_color", "Attribute Color", [] {
                                    return std::make_unique<AttributeColorRenderer>();
                                }});
        register_mesh_renderer({"balsa.unlit_wireframe", "Unlit Wireframe", [] {
                                    return std::make_unique<UnlitWireframeRenderer>();
                                }});
        register_mesh_renderer({"balsa.unlit_points", "Unlit Points", [] {
                                    return std::make_unique<UnlitPointsRenderer>();
                                }});
        return true;
    }();
    (void)once;
}

auto make_default_mesh_render_layers(std::size_t dimension)
    -> std::vector<MeshRenderLayer> {
    register_builtin_mesh_renderers();
    std::vector<MeshRenderLayer> layers;
    layers.emplace_back("Surface", create_mesh_renderer("balsa.phong_surface"));
    layers.back().view = views::surface_for(dimension);
    layers.emplace_back("Wireframe", create_mesh_renderer("balsa.unlit_wireframe"));
    layers.back().view = views::wireframe_for(dimension);
    layers.emplace_back("Points", create_mesh_renderer("balsa.unlit_points"));
    return layers;
}

} // namespace balsa::scene_graph
