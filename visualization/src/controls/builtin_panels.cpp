// Panels capture raw Object/BasicShading/Light pointers; rebuild layouts when
// the selection or features change.

#include <format>
#include <string>
#include <vector>

#include "balsa/scene_graph/Light.hpp"
#include "balsa/scene_graph/BasicShading.hpp"
#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/MeshInstance.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/visualization/colormap_list.hpp"
#include "balsa/visualization/controls/Panel.hpp"

namespace balsa::visualization::controls {

namespace sg = scene_graph;
using vulkan::ColorSource;
using vulkan::CullMode;
using vulkan::NormalSource;
using vulkan::ShadingModel;

namespace {

auto has_active_shading(const PanelContext &ctx) -> bool {
    return ctx.active_shading() != nullptr;
}

auto is_lit(const sg::BasicShading &shading) -> bool {
    return shading.render_state().normal_source != NormalSource::None;
}

auto make_property(std::string label, auto widget) -> Property {
    return Property{ .label = std::move(label), .widget = std::move(widget) };
}

template <typename E>
auto choice_input(std::vector<std::string> options,
                E &field,
                std::function<void()> after = {}) -> ChoiceInput {
    return {
        .options = [options = std::move(options)] { return options; },
        .get_index = [&field] { return static_cast<int>(field); },
        .set_index =
            [&field, after = std::move(after)](const int &i) {
                field = static_cast<E>(i);
                if (after) after();
            },
    };
}

auto float_input(float &field, float min, float max, float step,
                 bool use_slider = false) -> FloatInput {
    return { .get = [&field] { return field; },
             .set = [&field](const float &v) { field = v; },
             .min = min,
             .max = max,
             .step = step,
             .use_slider = use_slider };
}

auto bool_property(std::string label, bool &field) -> Property {
    return make_property(std::move(label),
                BoolInput{ .get = [&field] { return field; },
                           .set = [&field](const bool &v) { field = v; } });
}

auto color_property(std::string label, sg::Vec4f &field) -> Property {
    return make_property(std::move(label),
                ColorInput{ .get = [&field] { return field; },
                            .set = [&field](const sg::Vec4f &v) { field = v; } });
}

auto vec3_property(std::string label, Getter<sg::Vec3f> get, Setter<sg::Vec3f> set,
               float min = -1e6f, float max = 1e6f, float step = 0.1f)
    -> Property {
    return make_property(std::move(label), Vec3Input{ .get = std::move(get),
                                              .set = std::move(set),
                                              .min = min,
                                              .max = max,
                                              .step = step });
}

auto button_property(std::string label, std::function<void()> run) -> Property {
    return make_property(std::move(label), Button{ .on_click = std::move(run) });
}

// Role choices: option 0 is "(none)", option k is the k-th user
// attribute of the shading's geometry (reserved topology attributes are
// not offered).

auto attribute_handles(const sg::BasicShading &shading)
    -> std::vector<sg::MeshAttributeHandle> {
    const auto *geometry = shading.geometry();
    if (geometry == nullptr) return {};
    return std::ranges::to<std::vector>(
        geometry->attributes()
        | std::views::filter([](const sg::MeshAttributeHandle &a) {
              return !sg::MeshGeometry::is_reserved_name(a->name);
          }));
}

auto attribute_names(const sg::BasicShading &shading)
    -> std::vector<std::string> {
    std::vector<std::string> names{ "(none)" };
    for (const auto &a : attribute_handles(shading)) names.push_back(a->name);
    return names;
}

auto bound_index(const sg::BasicShading &shading, const sg::RoleBinding &b)
    -> int {
    if (!b.is_bound()) return 0;
    const auto handles = attribute_handles(shading);
    const auto it = std::ranges::find(handles, b.attribute());
    return it == handles.end()
               ? 0
               : static_cast<int>(std::distance(handles.begin(), it)) + 1;
}

template <typename Assign, typename Clear>
auto role_property(std::string label, sg::BasicShading &shading,
                   const sg::RoleBinding &(sg::BasicShading::*binding)() const,
                   Assign assign, Clear clear) -> Property {
    return make_property(
        std::move(label),
        ChoiceInput{
            .options = [&shading] { return attribute_names(shading); },
            .get_index =
                [&shading, binding] {
                    return bound_index(shading, (shading.*binding)());
                },
            .set_index =
                [&shading, assign, clear](const int &i) {
                    const auto handles = attribute_handles(shading);
                    if (i <= 0) {
                        clear(shading);
                    } else if (static_cast<std::size_t>(i - 1) < handles.size()) {
                        assign(shading, handles[static_cast<std::size_t>(i - 1)]);
                    }
                    shading.render_state().constrain(shading.has_normals());
                },
        });
}

// ── Object tab ──────────────────────────────────────────────────────

auto fill_object(sg::Object &object, Layout &layout) -> void {
    auto *obj = &object;
    layout.add_property(make_property("Name",
                     TextInput{ .get = [obj] { return obj->name; },
                                  .set = [obj](const std::string &s) {
                                      obj->name = s;
                                  } }));
    layout.add_property(bool_property("Visible", obj->visible));

    auto &info = layout.add_column("Mesh");
    auto geometry_of = [obj]() -> const sg::MeshGeometry * {
        const auto *instance = obj->find_feature<sg::MeshInstance>();
        return instance ? instance->geometry() : nullptr;
    };
    info.set_visible_when([geometry_of] { return geometry_of() != nullptr; });
    auto count = [](std::string what, auto get) {
        return make_property(std::move(what), ReadOnlyText{ .get = [get] {
                        const auto n = get();
                        return n ? std::to_string(*n) : std::string{};
                    } });
    };
    info.add_property(count("Vertices", [obj] {
        const auto *shading = obj->find_feature<sg::BasicShading>();
        return shading ? std::optional(shading->vertex_count()) : std::nullopt;
    }));
    info.add_property(count("Triangles", [geometry_of] {
        const auto *g = geometry_of();
        return g ? std::optional(g->triangle_count()) : std::nullopt;
    }));
    info.add_property(count("Edges", [geometry_of] {
        const auto *g = geometry_of();
        return g ? std::optional(g->edge_count()) : std::nullopt;
    }));
}

auto fill_transform(sg::Object &object, Layout &layout) -> void {
    auto *obj = &object;
    layout.add_property(vec3_property(
        "Translation", [obj] { return obj->translation(); },
        [obj](const sg::Vec3f &v) { obj->set_translation(v); }));
    auto rotation = vec3_property(
        "Rotation", [obj] { return obj->rotation_euler(); },
        [obj](const sg::Vec3f &v) { obj->set_rotation_euler(v); }, -360.0f,
        360.0f, 1.0f);
    rotation.tooltip = "Euler XYZ, degrees";
    layout.add_property(std::move(rotation));
    layout.add_property(vec3_property(
        "Scale", [obj] { return obj->scale_factors(); },
        [obj](const sg::Vec3f &v) { obj->set_scale_factors(v); }));
    layout.add_property(button_property("Reset Transform", [obj] { obj->reset_transform(); }));
}

// ── Data tab ────────────────────────────────────────────────────────

auto fill_render_layers(sg::BasicShading &shading, Layout &layout) -> void {
    auto &layers = shading.render_state().layers;

    auto &solid = layout.add_row("Solid");
    solid.add_property(bool_property("Enabled", layers.solid.enabled));
    solid.add_property(color_property("Color", layers.solid.color));

    auto &wire = layout.add_row("Wireframe");
    wire.add_property(bool_property("Enabled", layers.wireframe.enabled));
    wire.add_property(color_property("Color", layers.wireframe.color));
    auto width = make_property("Width", float_input(layers.wireframe.width, 0.5f, 5.0f, 0.1f, true));
    width.enabled_when = [&layers] { return layers.wireframe.enabled; };
    wire.add_property(std::move(width));

    auto &pts = layout.add_row("Points");
    pts.add_property(bool_property("Enabled", layers.points.enabled));
    pts.add_property(color_property("Color", layers.points.color));
    auto size = make_property("Size", float_input(layers.points.size, 0.1f, 20.0f, 0.1f, true));
    size.enabled_when = [&layers] { return layers.points.enabled; };
    pts.add_property(std::move(size));
}

auto fill_shading(sg::BasicShading &shading, Layout &layout) -> void {
    auto *sh = &shading;
    auto &rs = sh->render_state();
    auto constrain = [sh] { sh->render_state().constrain(sh->has_normals()); };
    constrain(); // state may have been set before normals were known

    auto normals = choice_input<NormalSource>(
        { "From Attribute", "Computed (flat)", "None (unlit)" },
        rs.normal_source, constrain);
    normals.option_enabled = [sh](int i) {
        return i != static_cast<int>(NormalSource::FromAttribute)
               || sh->has_normals();
    };
    layout.add_property(make_property("Normals", std::move(normals)));

    auto model = choice_input<ShadingModel>({ "Flat", "Gouraud", "Phong" },
                                            rs.shading, constrain);
    model.option_enabled = [sh](int i) {
        return i == static_cast<int>(ShadingModel::Flat)
               || sh->render_state().normal_source
                      == NormalSource::FromAttribute;
    };
    auto model_property = make_property("Shading", std::move(model));
    model_property.visible_when = [sh] { return is_lit(*sh); };
    layout.add_property(std::move(model_property));

    auto two_sided = bool_property("Two-sided Lighting", rs.two_sided);
    two_sided.visible_when = [sh] { return is_lit(*sh); };
    layout.add_property(std::move(two_sided));

    layout.add_property(make_property("Face Culling",
                     choice_input<CullMode>({ "None", "Back", "Front" }, rs.cull_mode)));
}

auto fill_color(sg::BasicShading &shading, Layout &layout) -> void {
    auto *sh = &shading;
    auto &rs = sh->render_state();

    auto source = choice_input<ColorSource>(
        { "Uniform", "Per-Vertex", "Scalar Field" }, rs.color_source);
    source.option_enabled = [sh](int i) {
        return i != static_cast<int>(ColorSource::ScalarField)
               || sh->has_scalar_field();
    };
    layout.add_property(make_property("Source", std::move(source)));

    auto uniform = color_property("Color", rs.uniform_color);
    uniform.visible_when = [&rs] { return rs.color_source == ColorSource::Uniform; };
    layout.add_property(std::move(uniform));

    auto &scalar = layout.add_column("Scalar Field");
    scalar.set_visible_when(
        [&rs] { return rs.color_source == ColorSource::ScalarField; });

    scalar.add_property(make_property(
        "Colormap",
        ChoiceInput{
            .options =
                [] {
                    const auto names = shaders::colormap_names();
                    return std::vector<std::string>(names.begin(), names.end());
                },
            .get_index = [&rs] { return find_colormap_index(rs.colormap_name); },
            .set_index =
                [&rs](const int &i) {
                    const auto names = shaders::colormap_names();
                    if (i >= 0 && static_cast<std::size_t>(i) < names.size()) {
                        rs.colormap_name =
                            std::string(names[static_cast<std::size_t>(i)]);
                    }
                },
        }));
    scalar.add_property(make_property("Colormap Name",
                     TextInput{ .get = [&rs] { return rs.colormap_name; },
                                  .set = [&rs](const std::string &s) {
                                      rs.colormap_name = s;
                                  } }));

    auto &range = scalar.add_row("Range");
    range.add_property(make_property("Min", float_input(rs.scalar_min, -1e6f, 1e6f, 0.1f)));
    range.add_property(make_property("Max", float_input(rs.scalar_max, -1e6f, 1e6f, 0.1f)));
    range.add_property(button_property("Reset", [&rs] {
        rs.scalar_min = 0.0f;
        rs.scalar_max = 1.0f;
    }));
}

auto fill_attributes(sg::BasicShading &shading, Layout &layout) -> void {
    auto *sh = &shading;
    layout.add_property(role_property(
        "Position", *sh, &sg::BasicShading::position_binding,
        [](sg::BasicShading &s, const sg::MeshAttributeHandle &a) { s.assign_position(a); },
        [](sg::BasicShading &s) { s.clear_position(); }));
    layout.add_property(role_property(
        "Normal", *sh, &sg::BasicShading::normal_binding,
        [](sg::BasicShading &s, const sg::MeshAttributeHandle &a) { s.assign_normal(a); },
        [](sg::BasicShading &s) { s.clear_normal(); }));
    layout.add_property(role_property(
        "Scalar", *sh, &sg::BasicShading::scalar_binding,
        [](sg::BasicShading &s, const sg::MeshAttributeHandle &a) {
            s.assign_scalar(a, s.scalar_component());
        },
        [](sg::BasicShading &s) { s.clear_scalar(); }));

    auto component = make_property(
        "Component",
        ChoiceInput{
            .options =
                [sh] {
                    std::vector<std::string> items{ "Magnitude" };
                    const auto &b = sh->scalar_binding();
                    const int n = b.is_bound() ? b.attribute()->component_count() : 0;
                    constexpr std::string_view axes = "XYZW";
                    for (int c = 0; c < n; ++c) {
                        items.emplace_back(
                            c < 4 ? std::string(1, axes[static_cast<std::size_t>(c)])
                                  : std::format("[{}]", c));
                    }
                    return items;
                },
            .get_index = [sh] { return sh->scalar_component() + 1; },
            .set_index = [sh](const int &i) { sh->set_scalar_component(i - 1); },
        });
    component.visible_when = [sh] {
        const auto &b = sh->scalar_binding();
        return b.is_bound() && b.attribute()->component_count() > 1;
    };
    layout.add_property(std::move(component));
}

// ── Material tab ────────────────────────────────────────────────────

auto fill_material(sg::BasicShading &shading, Layout &layout) -> void {
    auto &m = shading.render_state().material;
    layout.add_property(make_property("Ambient", float_input(m.ambient_strength, 0.0f, 1.0f, 0.01f, true)));
    layout.add_property(make_property("Diffuse", float_input(m.diffuse_strength, 0.0f, 2.0f, 0.01f, true)));
    layout.add_property(make_property("Specular", float_input(m.specular_strength, 0.0f, 2.0f, 0.01f, true)));
    layout.add_property(make_property("Shininess", float_input(m.shininess, 1.0f, 256.0f, 1.0f, true)));
}

// ── Scene tab ───────────────────────────────────────────────────────

auto fill_lighting(sg::Light &headlight, Layout &layout) -> void {
    auto *light = &headlight;
    layout.add_property(bool_property("Enabled", light->enabled));

    auto dir = vec3_property(
        "Direction", [light] { return light->direction; },
        [light](const sg::Vec3f &v) { light->direction = v; }, -1.0f, 1.0f,
        0.05f);
    dir.tooltip = "Camera space";
    layout.add_property(std::move(dir));

    layout.add_property(make_property(
        "Color",
        ColorInput{ .get =
                        [light] {
                            const auto &c = light->color;
                            return sg::Vec4f{ c(0), c(1), c(2), 1.0f };
                        },
                    .set =
                        [light](const sg::Vec4f &c) {
                            light->color = sg::Vec3f(c.head<3>());
                        },
                    .show_alpha = false }));
}

// Adapt a fill function taking the required state by reference; the
// panel's is_available() guarantees that state exists.
auto for_active_object(void (*fill)(sg::Object &, Layout &)) {
    return [fill](const PanelContext &c, Layout &l) { fill(*c.active_object, l); };
}
auto for_active_shading(void (*fill)(sg::BasicShading &, Layout &)) {
    return [fill](const PanelContext &c, Layout &l) { fill(*c.active_shading(), l); };
}
auto for_headlight(void (*fill)(sg::Light &, Layout &)) {
    return [fill](const PanelContext &c, Layout &l) { fill(*c.headlight(), l); };
}

auto has_active_object(const PanelContext &c) -> bool {
    return c.active_object != nullptr;
}
auto has_lit_shading(const PanelContext &c) -> bool {
    const auto *shading = c.active_shading();
    return shading && is_lit(*shading);
}
auto has_headlight(const PanelContext &c) -> bool {
    return c.headlight() != nullptr;
}

} // namespace

auto register_builtin_panels() -> void {
    static const bool once = [] {
        register_tab({ .id = "object", .label = "Object", .icon_glyph = "\u25A2", .sort_key = 0 });     // ▢
        register_tab({ .id = "data", .label = "Data", .icon_glyph = "\u25B3", .sort_key = 10 });        // △
        register_tab({ .id = "material", .label = "Material", .icon_glyph = "\u25CF", .sort_key = 20 }); // ●
        register_tab({ .id = "scene", .label = "Scene", .icon_glyph = "\u2600", .sort_key = 30 });      // ☀

        register_panel({ .id = "object.object",
                         .tab_id = "object",
                         .sort_key = 0,
                         .label = "Object",
                         .is_available = has_active_object,
                         .fill_layout = for_active_object(fill_object) });
        register_panel({ .id = "object.transform",
                         .tab_id = "object",
                         .sort_key = 10,
                         .label = "Transform",
                         .is_available = has_active_object,
                         .fill_layout = for_active_object(fill_transform) });
        register_panel({ .id = "data.render_layers",
                         .tab_id = "data",
                         .sort_key = 0,
                         .label = "Render Layers",
                         .is_available = has_active_shading,
                         .fill_layout = for_active_shading(fill_render_layers) });
        register_panel({ .id = "data.shading",
                         .tab_id = "data",
                         .sort_key = 10,
                         .label = "Shading & Rendering",
                         .is_available = has_active_shading,
                         .fill_layout = for_active_shading(fill_shading) });
        register_panel({ .id = "data.color",
                         .tab_id = "data",
                         .sort_key = 20,
                         .label = "Color",
                         .is_available = has_active_shading,
                         .fill_layout = for_active_shading(fill_color) });
        register_panel({ .id = "data.attributes",
                         .tab_id = "data",
                         .sort_key = 30,
                         .label = "Attribute Bindings",
                         .expanded_by_default = false,
                         .is_available = has_active_shading,
                         .fill_layout = for_active_shading(fill_attributes) });
        register_panel({ .id = "material.response",
                         .tab_id = "material",
                         .sort_key = 0,
                         .label = "Material Response",
                         .is_available = has_lit_shading,
                         .fill_layout = for_active_shading(fill_material) });
        register_panel({ .id = "scene.lighting",
                         .tab_id = "scene",
                         .sort_key = 0,
                         .label = "Lighting",
                         .is_available = has_headlight,
                         .fill_layout = for_headlight(fill_lighting) });
        return true;
    }();
    (void)once;
}

} // namespace balsa::visualization::controls
