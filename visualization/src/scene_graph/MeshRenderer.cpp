#include "balsa/scene_graph/MeshRenderer.hpp"

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

auto position_requirement() -> MeshRoleRequirement {
    return {.id = std::string(position_role),
            .min_components = 1,
            .max_components = 4,
            .default_attribute = "vertex_positions"};
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
        case 1: return extents_of(data.colwise().template norm<1>());
        case 3: return extents_of(data.colwise().template norm<3>());
        default: return extents_of(data.colwise().template norm<2>());
        }
    });
}

auto MeshRenderer::requirement(std::string_view role) const
    -> const MeshRoleRequirement * {
    const auto roles = required_roles();
    const auto it = std::ranges::find(roles, role, &MeshRoleRequirement::id);
    return it == roles.end() ? nullptr : &*it;
}

MeshRenderLayer::MeshRenderLayer(const MeshRenderLayer &other)
    : id(other.id), name(other.name), enabled(other.enabled),
      renderer(other.renderer ? other.renderer->clone() : nullptr),
      bindings(other.bindings) {}

auto MeshRenderLayer::operator=(const MeshRenderLayer &other) -> MeshRenderLayer & {
    if (this == &other) return *this;
    id = other.id;
    name = other.name;
    enabled = other.enabled;
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
    static const std::array roles{
        position_requirement(),
        MeshRoleRequirement{.id = std::string(normal_role),
                            .min_components = 2,
                            .max_components = 4,
                            .required = false,
                            .default_attribute = "vertex_normals"},
        // Up to 4 raw components; scalar_colormap.reduction picks the scalar.
        MeshRoleRequirement{.id = std::string(scalar_role),
                            .min_components = 1,
                            .max_components = 4,
                            .required = false},
    };
    return roles;
}
auto PhongSurfaceRenderer::draw_request() const -> MeshDrawRequest {
    return {.primitive = MeshPrimitive::Triangles,
            .state = state,
            .colormap = scalar_colormap.colormap};
}
auto PhongSurfaceRenderer::scalar_data_extents(const MeshLayerContext &layer) const
    -> std::optional<std::array<float, 2>> {
    const auto attribute = layer.attribute(scalar_role);
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
    material.uniform_color = state.uniform_color;
    material.scalar_reduction = scalar_colormap.reduction;
    const auto [min, max] = scalar_range(layer);
    material.scalar_min = min;
    material.scalar_max = max;
}
auto PhongSurfaceRenderer::build_controls(const MeshLayerContext &layer)
    -> visualization::controls::Layout {
    using namespace visualization::controls;
    Layout layout;
    auto &scalar = scalar_colormap;
    auto uniform = color_property("Color", state.uniform_color);
    // Bound scalars replace the uniform colour.
    auto scalar_bound = [layer] { return layer.attribute(scalar_role).valid(); };
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
    static const std::array roles{position_requirement()};
    return roles;
}
auto UnlitWireframeRenderer::draw_request() const -> MeshDrawRequest {
    visualization::vulkan::MeshRenderState state;
    state.normal_source = visualization::vulkan::NormalSource::None;
    return {.primitive = MeshPrimitive::Edges, .state = std::move(state)};
}
auto UnlitWireframeRenderer::write_material(MeshMaterial &material,
                                     const MeshLayerContext &) const -> void {
    material.layer_color = color;
    material.primitive_size = width;
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
    static const std::array roles{position_requirement()};
    return roles;
}
auto UnlitPointsRenderer::draw_request() const -> MeshDrawRequest {
    visualization::vulkan::MeshRenderState state;
    state.normal_source = visualization::vulkan::NormalSource::None;
    return {.primitive = MeshPrimitive::Vertices, .state = std::move(state)};
}
auto UnlitPointsRenderer::write_material(MeshMaterial &material,
                                     const MeshLayerContext &) const -> void {
    material.layer_color = color;
    material.primitive_size = size;
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

auto register_builtin_mesh_renderers() -> void {
    static const bool once = [] {
        register_mesh_renderer({"balsa.phong_surface", "Phong Surface", [] {
                                    return std::make_unique<PhongSurfaceRenderer>();
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

auto make_default_mesh_render_layers() -> std::vector<MeshRenderLayer> {
    register_builtin_mesh_renderers();
    std::vector<MeshRenderLayer> layers;
    layers.emplace_back("Surface", create_mesh_renderer("balsa.phong_surface"));
    layers.emplace_back("Wireframe", create_mesh_renderer("balsa.unlit_wireframe"));
    layers.emplace_back("Points", create_mesh_renderer("balsa.unlit_points"));
    return layers;
}

} // namespace balsa::scene_graph
