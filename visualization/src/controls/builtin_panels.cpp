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
#include "balsa/visualization/controls/Panel.hpp"

namespace balsa::visualization::controls {

namespace sg = scene_graph;
using vulkan::CullMode;
using vulkan::NormalSource;
using vulkan::ShadingModel;

namespace {

auto has_active_shading(const PanelContext &ctx) -> bool {
    return ctx.active_shading() != nullptr;
}

auto surface_renderer(const sg::MeshRenderLayer *layer) -> sg::PhongSurfaceRenderer * {
    return layer ? dynamic_cast<sg::PhongSurfaceRenderer *>(layer->renderer.get())
                 : nullptr;
}

auto is_lit(const sg::BasicShading &shading) -> bool {
    const auto *renderer = surface_renderer(shading.find_layer("balsa.phong_surface"));
    return renderer && renderer->state.normal_source != NormalSource::None;
}

// Whether role of layer resolves to an attribute of the shading's geometry.
auto has_role(const sg::BasicShading &shading, const sg::MeshRenderLayer &layer,
              std::string_view role) -> bool {
    return shading.layer_attribute(layer, role).valid();
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

// Role choices: option 0 is the renderer's default, option 1 "(none)",
// option k + 2 the k-th user attribute of the shading's geometry
// (reserved topology attributes are not offered).

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

auto role_property(sg::BasicShading &shading, sg::MeshLayerId layer_id,
                   const sg::MeshRoleRequirement &requirement) -> Property {
    auto *sh = &shading;
    const std::string role = requirement.id;
    const std::string default_label =
        requirement.default_attribute.empty()
            ? std::string("(default: none)")
            : std::format("(default: {})", requirement.default_attribute);
    auto property = make_property(
        role,
        ChoiceInput{
            .options =
                [sh, default_label] {
                    std::vector<std::string> names{default_label, "(none)"};
                    for (const auto &a : attribute_handles(*sh)) names.push_back(a->name);
                    return names;
                },
            .get_index =
                [sh, layer_id, role] {
                    const auto *layer = sh->find_layer(layer_id);
                    const auto *binding = layer ? layer->binding(role) : nullptr;
                    if (binding == nullptr) return 0;
                    if (!binding->attribute) return 1;
                    const auto handles = attribute_handles(*sh);
                    const auto it = std::ranges::find(handles, binding->attribute->name,
                                                      [](const auto &a) { return a->name; });
                    return it == handles.end()
                               ? 1
                               : static_cast<int>(std::distance(handles.begin(), it)) + 2;
                },
            .set_index =
                [sh, layer_id, role](const int &i) {
                    const auto handles = attribute_handles(*sh);
                    if (i <= 0) {
                        sh->reset_layer_role(layer_id, role);
                    } else if (i == 1) {
                        sh->clear_layer_role(layer_id, role);
                    } else if (static_cast<std::size_t>(i - 2) < handles.size()) {
                        sh->assign_layer_role(layer_id, role,
                                              handles[static_cast<std::size_t>(i - 2)]);
                    }
                    auto *layer = sh->find_layer(layer_id);
                    if (auto *renderer = surface_renderer(layer)) {
                        renderer->state.constrain(has_role(*sh, *layer, sg::normal_role));
                    }
                },
            .option_enabled =
                [sh, requirement](int i) {
                    if (i < 2) return true;
                    const auto handles = attribute_handles(*sh);
                    const auto k = static_cast<std::size_t>(i - 2);
                    return k < handles.size() && requirement.accepts(*handles[k]);
                },
        });
    property.tooltip = std::format("{} attribute, {}-{} components",
                                   requirement.domain == sg::AttributeDomain::Vertex ? "vertex"
                                   : requirement.domain == sg::AttributeDomain::Edge ? "edge"
                                                                                     : "face",
                                   requirement.min_components,
                                   requirement.max_components);
    return property;
}

// ── Object tab ──────────────────────────────────────────────────────

auto object_layout(sg::Object &object) -> Layout {
    Layout layout;
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
    info.add_property(count("Vertices", [obj]() -> std::optional<std::size_t> {
        const auto *shading = obj->find_feature<sg::BasicShading>();
        if (shading == nullptr) return std::nullopt;
        for (const auto &layer : shading->render_layers()) {
            if (auto p = shading->layer_attribute(layer, sg::position_role)) {
                return p->count();
            }
        }
        return std::nullopt;
    }));
    info.add_property(count("Triangles", [geometry_of] {
        const auto *g = geometry_of();
        return g ? std::optional(g->triangle_count()) : std::nullopt;
    }));
    info.add_property(count("Edges", [geometry_of] {
        const auto *g = geometry_of();
        return g ? std::optional(g->edge_count()) : std::nullopt;
    }));
    return layout;
}

auto transform_layout(sg::Object &object) -> Layout {
    Layout layout;
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
    return layout;
}

// ── Data tab ────────────────────────────────────────────────────────

auto render_layers_layout(sg::BasicShading &shading) -> Layout {
    Layout layout;
    for (auto &layer : shading.render_layers()) {
        if (!layer.renderer) continue;
        auto renderer_layout =
            layer.renderer->build_controls(sg::MeshLayerContext(shading, layer.id));
        renderer_layout.add_property(bool_property("Enabled", layer.enabled));
        auto &bindings = renderer_layout.add_column("Bindings");
        for (const auto &requirement : layer.renderer->required_roles()) {
            bindings.add_property(role_property(shading, layer.id, requirement));
        }
        layout.add_layout(std::move(renderer_layout));
    }
    return layout;
}

auto shading_layout(sg::BasicShading &shading) -> Layout {
    Layout layout;
    auto *sh = &shading;
    auto *layer = sh->find_layer("balsa.phong_surface");
    auto *renderer = surface_renderer(layer);
    if (!renderer) return layout;
    auto &rs = renderer->state;
    const sg::MeshLayerId layer_id = layer->id;
    auto has_normals = [sh, layer_id] {
        const auto *l = sh->find_layer(layer_id);
        return l && has_role(*sh, *l, sg::normal_role);
    };
    auto constrain = [renderer, has_normals] { renderer->state.constrain(has_normals()); };
    constrain(); // state may have been set before normals were known

    auto normals = choice_input<NormalSource>(
        { "From Attribute", "Computed (flat)", "None (unlit)" },
        rs.normal_source, constrain);
    normals.option_enabled = [has_normals](int i) {
        return i != static_cast<int>(NormalSource::FromAttribute) || has_normals();
    };
    layout.add_property(make_property("Normals", std::move(normals)));

    auto model = choice_input<ShadingModel>({ "Flat", "Gouraud", "Phong" },
                                            rs.shading, constrain);
    model.option_enabled = [renderer](int i) {
        return i == static_cast<int>(ShadingModel::Flat)
               || renderer->state.normal_source
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
    return layout;
}

// ── Material tab ────────────────────────────────────────────────────

auto material_layout(sg::BasicShading &shading) -> Layout {
    Layout layout;
    auto *renderer = surface_renderer(shading.find_layer("balsa.phong_surface"));
    if (!renderer) return layout;
    auto &m = renderer->state.material;
    layout.add_property(make_property("Ambient", float_input(m.ambient_strength, 0.0f, 1.0f, 0.01f, true)));
    layout.add_property(make_property("Diffuse", float_input(m.diffuse_strength, 0.0f, 2.0f, 0.01f, true)));
    layout.add_property(make_property("Specular", float_input(m.specular_strength, 0.0f, 2.0f, 0.01f, true)));
    layout.add_property(make_property("Shininess", float_input(m.shininess, 1.0f, 256.0f, 1.0f, true)));
    return layout;
}

// ── Scene tab ───────────────────────────────────────────────────────

auto lighting_layout(sg::Light &headlight) -> Layout {
    Layout layout;
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
    return layout;
}

// Adapt a layout builder taking the required state by reference; the
// panel's is_available() guarantees that state exists.
auto for_active_object(Layout (*build)(sg::Object &)) {
    return [build](const PanelContext &c) { return build(*c.active_object); };
}
auto for_active_shading(Layout (*build)(sg::BasicShading &)) {
    return [build](const PanelContext &c) { return build(*c.active_shading()); };
}
auto for_headlight(Layout (*build)(sg::Light &)) {
    return [build](const PanelContext &c) { return build(*c.headlight()); };
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
                         .build_layout = for_active_object(object_layout) });
        register_panel({ .id = "object.transform",
                         .tab_id = "object",
                         .sort_key = 10,
                         .label = "Transform",
                         .is_available = has_active_object,
                         .build_layout = for_active_object(transform_layout) });
        register_panel({ .id = "data.render_layers",
                         .tab_id = "data",
                         .sort_key = 0,
                         .label = "Render Layers",
                         .is_available = has_active_shading,
                         .build_layout = for_active_shading(render_layers_layout) });
        register_panel({ .id = "data.shading",
                         .tab_id = "data",
                         .sort_key = 10,
                         .label = "Shading & Rendering",
                         .is_available = has_active_shading,
                         .build_layout = for_active_shading(shading_layout) });
        register_panel({ .id = "material.response",
                         .tab_id = "material",
                         .sort_key = 0,
                         .label = "Material Response",
                         .is_available = has_lit_shading,
                         .build_layout = for_active_shading(material_layout) });
        register_panel({ .id = "scene.lighting",
                         .tab_id = "scene",
                         .sort_key = 0,
                         .label = "Lighting",
                         .is_available = has_headlight,
                         .build_layout = for_headlight(lighting_layout) });
        return true;
    }();
    (void)once;
}

} // namespace balsa::visualization::controls
