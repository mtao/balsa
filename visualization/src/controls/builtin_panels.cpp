// Panels capture raw Object/BasicShading/Light pointers; rebuild layouts when
// the selection or features change.

#include <format>
#include <string>
#include <vector>

#include "balsa/scene_graph/AttributeSemantic.hpp"
#include "balsa/scene_graph/GeometryView.hpp"
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

// The active layer's surface renderer, or nullptr.
auto active_surface(const sg::BasicShading &shading) -> sg::PhongSurfaceRenderer * {
    return surface_renderer(shading.active_layer());
}

auto is_lit(const sg::BasicShading &shading) -> bool {
    const auto *renderer = active_surface(shading);
    return renderer && renderer->state.normal_source != NormalSource::None;
}

// A field that may move or vanish (e.g. in the active layer): looked up on
// each access, nullptr if absent.
template <typename T>
using FieldRef = std::function<T *()>;

// Whether role of layer resolves to an attribute of the shading's geometry.
auto has_role(const sg::BasicShading &shading, const sg::MeshRenderLayer &layer,
              sg::MeshRole role) -> bool {
    return shading.layer_attribute(layer, role).valid();
}

auto make_property(std::string label, auto widget) -> Property {
    return Property{ .label = std::move(label), .widget = std::move(widget) };
}

template <typename E>
auto choice_input(std::vector<std::string> options,
                FieldRef<E> field,
                std::function<void()> after = {}) -> ChoiceInput {
    return {
        .options = [options = std::move(options)] { return options; },
        .get_index = [field] {
            const auto *f = field();
            return f ? static_cast<int>(*f) : 0;
        },
        .set_index =
            [field, after = std::move(after)](const int &i) {
                auto *f = field();
                if (f == nullptr) return;
                *f = static_cast<E>(i);
                if (after) after();
            },
    };
}

auto float_input(FieldRef<float> field, float min, float max, float step,
                 bool use_slider = false) -> FloatInput {
    return { .get = [field] {
                 const auto *f = field();
                 return f ? *f : 0.0f;
             },
             .set = [field](const float &v) {
                 if (auto *f = field()) *f = v;
             },
             .min = min,
             .max = max,
             .step = step,
             .use_slider = use_slider };
}

auto bool_property(std::string label, FieldRef<bool> field) -> Property {
    return make_property(std::move(label),
                BoolInput{ .get = [field] {
                               const auto *f = field();
                               return f && *f;
                           },
                           .set = [field](const bool &v) {
                               if (auto *f = field()) *f = v;
                           } });
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
// option k + 2 the k-th candidate for the role (BasicShading::candidates).

auto role_candidates(const sg::BasicShading &shading, sg::MeshLayerId layer_id,
                     sg::MeshRole role) -> std::vector<sg::MeshAttributeHandle> {
    const auto *layer = shading.find_layer(layer_id);
    return layer ? shading.candidates(*layer, role)
                 : std::vector<sg::MeshAttributeHandle>{};
}

auto role_property(sg::BasicShading &shading, sg::MeshLayerId layer_id,
                   const sg::MeshRoleRequirement &requirement) -> Property {
    auto *sh = &shading;
    const sg::MeshRole role = requirement.role;
    const auto semantic = requirement.semantic;
    // "(default: Normal: vertex_normals)", or "(default: none)".
    const auto default_label = [sh, layer_id, semantic] {
        if (!semantic) return std::string("(default: none)");
        const auto *layer = sh->find_layer(layer_id);
        const auto geometry = layer ? sh->layer_geometry(*layer) : nullptr;
        const auto attribute =
            geometry ? geometry->attribute_for(*semantic) : sg::MeshAttributeHandle{};
        return std::format("(default: {}: {})",
                           semantic->value().label,
                           attribute ? attribute->name : "missing");
    };
    auto property = make_property(
        std::string(role.id()),
        ChoiceInput{
            .options =
                [sh, layer_id, role, default_label] {
                    std::vector<std::string> names{default_label(), "(none)"};
                    for (const auto &a : role_candidates(*sh, layer_id, role)) {
                        names.push_back(a->name);
                    }
                    return names;
                },
            .get_index =
                [sh, layer_id, role] {
                    const auto *layer = sh->find_layer(layer_id);
                    const auto *binding = layer ? layer->binding(role) : nullptr;
                    if (binding == nullptr) return 0;
                    if (!binding->attribute) return 1;
                    const auto handles = role_candidates(*sh, layer_id, role);
                    const auto it = std::ranges::find(handles, binding->attribute->name,
                                                      [](const auto &a) { return a->name; });
                    return it == handles.end()
                               ? 1
                               : static_cast<int>(std::distance(handles.begin(), it)) + 2;
                },
            .set_index =
                [sh, layer_id, role](const int &i) {
                    const auto handles = role_candidates(*sh, layer_id, role);
                    if (i <= 0) {
                        sh->reset_layer_role(layer_id, role);
                    } else if (i == 1) {
                        sh->clear_layer_role(layer_id, role);
                    } else if (static_cast<std::size_t>(i - 2) < handles.size()) {
                        sh->assign_layer_role(layer_id, role,
                                              handles[static_cast<std::size_t>(i - 2)]);
                    }
                    // Let the renderer adapt to its new inputs.
                    if (auto *layer = sh->find_layer(layer_id); layer && layer->renderer) {
                        layer->renderer->fit_to({*sh, layer_id});
                    }
                },
        });
    property.tooltip = std::format("{} attribute, {}-{} components",
                                   requirement.domain == sg::AttributeDomain::Vertex ? "vertex"
                                   : requirement.domain == sg::AttributeDomain::Edge ? "edge"
                                   : requirement.domain == sg::AttributeDomain::Cell ? "cell"
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
    layout.add_property(make_property(
        "Visible",
        BoolInput{ .get = [obj] { return obj->visible(); },
                   .set = [obj](const bool &v) { obj->set_visible(v); } }));

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
            if (auto p = shading->layer_attribute(layer, sg::roles::position())) {
                return p->count();
            }
        }
        return std::nullopt;
    }));
    info.add_property(count("Triangles", [geometry_of] {
        const auto *g = geometry_of();
        return g ? std::optional(g->triangle_count()) : std::nullopt;
    }));
    info.add_property(count("Tetrahedra", [geometry_of] {
        const auto *g = geometry_of();
        return g ? std::optional(g->simplex_count(3)) : std::nullopt;
    }));
    // Its own edges, else those of its highest dimension.
    info.add_property(count("Edges", [obj]() -> std::optional<std::size_t> {
        const auto *instance = obj->find_feature<sg::MeshInstance>();
        if (instance == nullptr || instance->geometry() == nullptr) return std::nullopt;
        const auto edges = instance->view(sg::views::edges());
        return edges ? edges->edge_count() : 0;
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

// Which registered geometry view a layer draws.  Views with nothing to
// show for this mesh are disabled.
auto view_property(sg::BasicShading &shading, sg::MeshLayerId layer_id) -> Property {
    auto *sh = &shading;
    // Sorted by label.
    const auto views = [] {
        auto all = sg::geometry_views().all();
        std::ranges::sort(all, {}, [](sg::GeometryView v) { return v.value().label; });
        return all;
    };
    auto property = make_property(
        "View",
        ChoiceInput{
            .options =
                [views] {
                    return std::ranges::to<std::vector>(
                        views() | std::views::transform([](sg::GeometryView v) {
                            return v.value().label;
                        }));
                },
            .get_index =
                [sh, layer_id, views] {
                    const auto *layer = sh->find_layer(layer_id);
                    if (layer == nullptr) return 0;
                    const auto all = views();
                    return static_cast<int>(std::ranges::find(all, layer->view)
                                            - all.begin());
                },
            .set_index =
                [sh, layer_id, views](const int &i) {
                    const auto all = views();
                    auto *layer = sh->find_layer(layer_id);
                    if (layer == nullptr || i < 0
                        || static_cast<std::size_t>(i) >= all.size()) {
                        return;
                    }
                    layer->view = all[static_cast<std::size_t>(i)];
                    // Let the renderer adapt to the new geometry.
                    if (layer->renderer) layer->renderer->fit_to({*sh, layer_id});
                },
            .option_enabled =
                [sh, views](int i) {
                    const auto all = views();
                    if (i < 0 || static_cast<std::size_t>(i) >= all.size()
                        || !sh->is_attached()) {
                        return false;
                    }
                    const auto *instance = sh->object().find_feature<sg::MeshInstance>();
                    return instance != nullptr
                           && instance->view(all[static_cast<std::size_t>(i)]) != nullptr;
                },
        });
    property.tooltip = "The form of the mesh this layer draws";
    return property;
}

// Which layer the Shading and Material panels act on.
auto active_layer_property(sg::BasicShading &shading) -> Property {
    auto *sh = &shading;
    auto property = make_property(
        "Active Layer",
        ChoiceInput{
            .options =
                [sh] {
                    return std::ranges::to<std::vector>(
                        sh->render_layers()
                        | std::views::transform(&sg::MeshRenderLayer::name));
                },
            .get_index =
                [sh] {
                    const auto *active = sh->active_layer();
                    return active ? static_cast<int>(active - sh->render_layers().data())
                                  : 0;
                },
            .set_index =
                [sh](const int &i) {
                    const auto &layers = sh->render_layers();
                    if (i >= 0 && static_cast<std::size_t>(i) < layers.size()) {
                        sh->set_active_layer(layers[static_cast<std::size_t>(i)].id);
                    }
                },
        });
    property.tooltip = "The layer the Shading and Material panels edit";
    return property;
}

auto render_layers_layout(sg::BasicShading &shading) -> Layout {
    Layout layout;
    layout.add_property(active_layer_property(shading));
    for (auto &layer : shading.render_layers()) {
        if (!layer.renderer) continue;
        auto *sh = &shading;
        const auto id = layer.id;
        auto enabled = [sh, id] {
            const auto *l = sh->find_layer(id);
            return l && l->enabled;
        };
        // One headed group per layer: what it draws, then how.
        auto &group = layout.add_column(layer.name);
        group.add_property(bool_property("Enabled", [sh, id]() -> bool * {
            auto *l = sh->find_layer(id);
            return l ? &l->enabled : nullptr;
        }));
        auto view = view_property(shading, id);
        view.visible_when = enabled;
        group.add_property(std::move(view));
        group.add_layout(layer.renderer->build_controls(sg::MeshLayerContext(shading, id)))
            .set_visible_when(enabled);
        auto &bindings = group.add_column("Bindings");
        bindings.set_visible_when(enabled);
        for (const auto &requirement : layer.renderer->required_roles()) {
            bindings.add_property(role_property(shading, id, requirement));
        }
    }
    return layout;
}

auto shading_layout(sg::BasicShading &shading) -> Layout {
    Layout layout;
    auto *sh = &shading;
    // Read through the active layer on every access: it can change while
    // the panel is shown.
    auto state = [sh]() -> vulkan::MeshRenderState * {
        auto *renderer = active_surface(*sh);
        return renderer ? &renderer->state : nullptr;
    };
    auto has_normals = [sh] {
        const auto *l = sh->active_layer();
        return l && has_role(*sh, *l, sg::roles::normal());
    };
    auto constrain = [state, has_normals] {
        if (auto *s = state()) s->constrain(has_normals());
    };
    constrain(); // state may have been set before normals were known

    auto normals = choice_input<NormalSource>(
        { "From Attribute", "Computed (flat)", "None (unlit)" },
        [state] { return state() ? &state()->normal_source : nullptr; }, constrain);
    normals.option_enabled = [has_normals](int i) {
        return i != static_cast<int>(NormalSource::FromAttribute) || has_normals();
    };
    layout.add_property(make_property("Normals", std::move(normals)));

    auto model = choice_input<ShadingModel>(
        { "Flat", "Gouraud", "Phong" },
        [state] { return state() ? &state()->shading : nullptr; }, constrain);
    model.option_enabled = [state](int i) {
        const auto *s = state();
        return i == static_cast<int>(ShadingModel::Flat)
               || (s && s->normal_source == NormalSource::FromAttribute);
    };
    auto model_property = make_property("Shading", std::move(model));
    model_property.visible_when = [sh] { return is_lit(*sh); };
    layout.add_property(std::move(model_property));

    auto two_sided = bool_property("Two-sided Lighting", [state] {
        return state() ? &state()->two_sided : nullptr;
    });
    two_sided.visible_when = [sh] { return is_lit(*sh); };
    layout.add_property(std::move(two_sided));

    layout.add_property(make_property("Face Culling",
                     choice_input<CullMode>({ "None", "Back", "Front" }, [state] {
                         return state() ? &state()->cull_mode : nullptr;
                     })));
    return layout;
}

// ── Material tab ────────────────────────────────────────────────────

auto material_layout(sg::BasicShading &shading) -> Layout {
    Layout layout;
    auto *sh = &shading;
    // A material field of the active layer, looked up on each access.
    auto field = [sh](float sg::MaterialProperties::*member) -> FieldRef<float> {
        return [sh, member]() -> float * {
            auto *renderer = active_surface(*sh);
            return renderer ? &(renderer->state.material.*member) : nullptr;
        };
    };
    using M = sg::MaterialProperties;
    layout.add_property(make_property("Ambient", float_input(field(&M::ambient_strength), 0.0f, 1.0f, 0.01f, true)));
    layout.add_property(make_property("Diffuse", float_input(field(&M::diffuse_strength), 0.0f, 2.0f, 0.01f, true)));
    layout.add_property(make_property("Specular", float_input(field(&M::specular_strength), 0.0f, 2.0f, 0.01f, true)));
    layout.add_property(make_property("Shininess", float_input(field(&M::shininess), 1.0f, 256.0f, 1.0f, true)));
    return layout;
}

// ── Scene tab ───────────────────────────────────────────────────────

auto lighting_layout(sg::Light &headlight) -> Layout {
    Layout layout;
    auto *light = &headlight;
    layout.add_property(bool_property("Enabled", [light] { return &light->enabled; }));

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
auto has_active_surface(const PanelContext &c) -> bool {
    const auto *shading = c.active_shading();
    return shading && active_surface(*shading);
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
                         .is_available = has_active_surface,
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
