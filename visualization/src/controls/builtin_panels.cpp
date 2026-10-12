// Panels capture raw Object/MeshData/Light pointers; rebuild layouts when
// the selection or features change.

#include <format>
#include <string>
#include <vector>

#include "balsa/scene_graph/Light.hpp"
#include "balsa/scene_graph/MeshData.hpp"
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

auto has_mesh(const Context &ctx) -> bool { return ctx.mesh_data() != nullptr; }

auto is_lit(const sg::MeshData &md) -> bool {
    return md.render_state().normal_source != NormalSource::None;
}

auto prop(std::string label, auto value) -> Property {
    return Property{ .label = std::move(label), .value = std::move(value) };
}

template <typename E>
auto enum_value(std::vector<std::string> items,
                E &field,
                std::function<void()> after = {}) -> EnumValue {
    return {
        .items = [items = std::move(items)] { return items; },
        .get = [&field] { return static_cast<int>(field); },
        .set =
            [&field, after = std::move(after)](const int &i) {
                field = static_cast<E>(i);
                if (after) after();
            },
    };
}

auto float_value(float &field, float min, float max, float step,
                 bool slider = false) -> FloatValue {
    return { .get = [&field] { return field; },
             .set = [&field](const float &v) { field = v; },
             .min = min,
             .max = max,
             .step = step,
             .slider = slider };
}

auto bool_prop(std::string label, bool &field) -> Property {
    return prop(std::move(label),
                BoolValue{ .get = [&field] { return field; },
                           .set = [&field](const bool &v) { field = v; } });
}

auto color_prop(std::string label, sg::Vec4f &field) -> Property {
    return prop(std::move(label),
                ColorValue{ .get = [&field] { return field; },
                            .set = [&field](const sg::Vec4f &v) { field = v; } });
}

auto vec3_prop(std::string label, Getter<sg::Vec3f> get, Setter<sg::Vec3f> set,
               float min = -1e6f, float max = 1e6f, float step = 0.1f)
    -> Property {
    return prop(std::move(label), Vec3Value{ .get = std::move(get),
                                              .set = std::move(set),
                                              .min = min,
                                              .max = max,
                                              .step = step });
}

auto action_prop(std::string label, std::function<void()> run) -> Property {
    return prop(std::move(label), ActionValue{ .run = std::move(run) });
}

// Role enums: item 0 is "(none)", item k is attributes()[k - 1].

auto attribute_names(const sg::MeshData &md) -> std::vector<std::string> {
    std::vector<std::string> names{ "(none)" };
    for (const auto &a : md.attributes()) names.push_back(a->name);
    return names;
}

auto bound_index(const sg::MeshData &md, const sg::RoleBinding &b) -> int {
    if (!b.is_bound()) return 0;
    int i = 1;
    for (const auto &a : md.attributes()) {
        if (a == b.attribute()) return i;
        ++i;
    }
    return 0;
}

template <typename Assign, typename Clear>
auto role_prop(std::string label, sg::MeshData &md,
               const sg::RoleBinding &(sg::MeshData::*binding)() const,
               Assign assign, Clear clear) -> Property {
    return prop(
        std::move(label),
        EnumValue{
            .items = [&md] { return attribute_names(md); },
            .get = [&md, binding] { return bound_index(md, (md.*binding)()); },
            .set =
                [&md, assign, clear](const int &i) {
                    if (i <= 0) {
                        clear(md);
                    } else if (auto attrs = md.attributes();
                               static_cast<std::size_t>(i - 1)
                               < std::ranges::size(attrs)) {
                        assign(md, attrs[i - 1]);
                    }
                    md.render_state().constrain(md.has_normals());
                },
        });
}

// ── Object tab ──────────────────────────────────────────────────────

auto draw_object(sg::Object &object, Layout &layout) -> void {
    auto *obj = &object;
    layout.prop(prop("Name",
                     StringValue{ .get = [obj] { return obj->name; },
                                  .set = [obj](const std::string &s) {
                                      obj->name = s;
                                  } }));
    layout.prop(bool_prop("Visible", obj->visible));

    auto &info = layout.column("Mesh");
    info.set_visible([obj] { return obj->find_feature<sg::MeshData>() != nullptr; });
    auto count = [obj](std::string what, auto fn) {
        return prop(std::move(what), LabelValue{ .get = [obj, fn] {
                        auto *md = obj->find_feature<sg::MeshData>();
                        return md ? std::to_string(fn(*md)) : std::string{};
                    } });
    };
    info.prop(count("Vertices", [](const sg::MeshData &m) { return m.vertex_count(); }));
    info.prop(count("Triangles", [](const sg::MeshData &m) { return m.triangle_count(); }));
    info.prop(count("Edges", [](const sg::MeshData &m) { return m.edge_count(); }));
}

auto draw_transform(sg::Object &object, Layout &layout) -> void {
    auto *obj = &object;
    layout.prop(vec3_prop(
        "Translation", [obj] { return obj->translation(); },
        [obj](const sg::Vec3f &v) { obj->set_translation(v); }));
    auto rotation = vec3_prop(
        "Rotation", [obj] { return obj->rotation_euler(); },
        [obj](const sg::Vec3f &v) { obj->set_rotation_euler(v); }, -360.0f,
        360.0f, 1.0f);
    rotation.tooltip = "Euler XYZ, degrees";
    layout.prop(std::move(rotation));
    layout.prop(vec3_prop(
        "Scale", [obj] { return obj->scale_factors(); },
        [obj](const sg::Vec3f &v) { obj->set_scale_factors(v); }));
    layout.prop(action_prop("Reset Transform", [obj] { obj->reset_transform(); }));
}

// ── Data tab ────────────────────────────────────────────────────────

auto draw_render_layers(sg::MeshData &mesh, Layout &layout) -> void {
    auto &layers = mesh.render_state().layers;

    auto &solid = layout.row("Solid");
    solid.prop(bool_prop("Enabled", layers.solid.enabled));
    solid.prop(color_prop("Color", layers.solid.color));

    auto &wire = layout.row("Wireframe");
    wire.prop(bool_prop("Enabled", layers.wireframe.enabled));
    wire.prop(color_prop("Color", layers.wireframe.color));
    auto width = prop("Width", float_value(layers.wireframe.width, 0.5f, 5.0f, 0.1f, true));
    width.enabled = [&layers] { return layers.wireframe.enabled; };
    wire.prop(std::move(width));

    auto &pts = layout.row("Points");
    pts.prop(bool_prop("Enabled", layers.points.enabled));
    pts.prop(color_prop("Color", layers.points.color));
    auto size = prop("Size", float_value(layers.points.size, 0.1f, 20.0f, 0.1f, true));
    size.enabled = [&layers] { return layers.points.enabled; };
    pts.prop(std::move(size));
}

auto draw_shading(sg::MeshData &mesh, Layout &layout) -> void {
    auto *md = &mesh;
    auto &rs = md->render_state();
    auto constrain = [md] { md->render_state().constrain(md->has_normals()); };
    constrain(); // state may have been set before normals were known

    auto normals = enum_value<NormalSource>(
        { "From Attribute", "Computed (flat)", "None (unlit)" },
        rs.normal_source, constrain);
    normals.item_enabled = [md](int i) {
        return i != static_cast<int>(NormalSource::FromAttribute)
               || md->has_normals();
    };
    layout.prop(prop("Normals", std::move(normals)));

    auto shading = enum_value<ShadingModel>({ "Flat", "Gouraud", "Phong" },
                                            rs.shading, constrain);
    shading.item_enabled = [md](int i) {
        return i == static_cast<int>(ShadingModel::Flat)
               || md->render_state().normal_source
                      == NormalSource::FromAttribute;
    };
    auto shading_prop = prop("Shading", std::move(shading));
    shading_prop.visible = [md] { return is_lit(*md); };
    layout.prop(std::move(shading_prop));

    auto two_sided = bool_prop("Two-sided Lighting", rs.two_sided);
    two_sided.visible = [md] { return is_lit(*md); };
    layout.prop(std::move(two_sided));

    layout.prop(prop("Face Culling",
                     enum_value<CullMode>({ "None", "Back", "Front" }, rs.cull_mode)));
}

auto draw_color(sg::MeshData &mesh, Layout &layout) -> void {
    auto *md = &mesh;
    auto &rs = md->render_state();

    auto source = enum_value<ColorSource>(
        { "Uniform", "Per-Vertex", "Scalar Field" }, rs.color_source);
    source.item_enabled = [md](int i) {
        return i != static_cast<int>(ColorSource::ScalarField)
               || md->has_scalar_field();
    };
    layout.prop(prop("Source", std::move(source)));

    auto uniform = color_prop("Color", rs.uniform_color);
    uniform.visible = [&rs] { return rs.color_source == ColorSource::Uniform; };
    layout.prop(std::move(uniform));

    auto &scalar = layout.column("Scalar Field");
    scalar.set_visible(
        [&rs] { return rs.color_source == ColorSource::ScalarField; });

    scalar.prop(prop(
        "Colormap",
        EnumValue{
            .items =
                [] {
                    const auto names = shaders::colormap_names();
                    return std::vector<std::string>(names.begin(), names.end());
                },
            .get = [&rs] { return find_colormap_index(rs.colormap_name); },
            .set =
                [&rs](const int &i) {
                    const auto names = shaders::colormap_names();
                    if (i >= 0 && static_cast<std::size_t>(i) < names.size()) {
                        rs.colormap_name =
                            std::string(names[static_cast<std::size_t>(i)]);
                    }
                },
        }));
    scalar.prop(prop("Colormap Name",
                     StringValue{ .get = [&rs] { return rs.colormap_name; },
                                  .set = [&rs](const std::string &s) {
                                      rs.colormap_name = s;
                                  } }));

    auto &range = scalar.row("Range");
    range.prop(prop("Min", float_value(rs.scalar_min, -1e6f, 1e6f, 0.1f)));
    range.prop(prop("Max", float_value(rs.scalar_max, -1e6f, 1e6f, 0.1f)));
    range.prop(action_prop("Reset", [&rs] {
        rs.scalar_min = 0.0f;
        rs.scalar_max = 1.0f;
    }));
}

auto draw_attributes(sg::MeshData &mesh, Layout &layout) -> void {
    auto *md = &mesh;
    layout.prop(role_prop(
        "Position", *md, &sg::MeshData::position_binding,
        [](sg::MeshData &m, const sg::MeshAttributeHandle &a) { m.assign_position(a); },
        [](sg::MeshData &m) { m.clear_position(); }));
    layout.prop(role_prop(
        "Normal", *md, &sg::MeshData::normal_binding,
        [](sg::MeshData &m, const sg::MeshAttributeHandle &a) { m.assign_normal(a); },
        [](sg::MeshData &m) { m.clear_normal(); }));
    layout.prop(role_prop(
        "Scalar", *md, &sg::MeshData::scalar_binding,
        [](sg::MeshData &m, const sg::MeshAttributeHandle &a) {
            m.assign_scalar(a, m.scalar_component());
        },
        [](sg::MeshData &m) { m.clear_scalar(); }));

    auto component = prop(
        "Component",
        EnumValue{
            .items =
                [md] {
                    std::vector<std::string> items{ "Magnitude" };
                    const auto &b = md->scalar_binding();
                    const int n = b.is_bound() ? b.attribute()->component_count() : 0;
                    constexpr std::string_view axes = "XYZW";
                    for (int c = 0; c < n; ++c) {
                        items.emplace_back(
                            c < 4 ? std::string(1, axes[static_cast<std::size_t>(c)])
                                  : std::format("[{}]", c));
                    }
                    return items;
                },
            .get = [md] { return md->scalar_component() + 1; },
            .set = [md](const int &i) { md->set_scalar_component(i - 1); },
        });
    component.visible = [md] {
        const auto &b = md->scalar_binding();
        return b.is_bound() && b.attribute()->component_count() > 1;
    };
    layout.prop(std::move(component));
}

// ── Material tab ────────────────────────────────────────────────────

auto draw_material(sg::MeshData &mesh, Layout &layout) -> void {
    auto &m = mesh.render_state().material;
    layout.prop(prop("Ambient", float_value(m.ambient_strength, 0.0f, 1.0f, 0.01f, true)));
    layout.prop(prop("Diffuse", float_value(m.diffuse_strength, 0.0f, 2.0f, 0.01f, true)));
    layout.prop(prop("Specular", float_value(m.specular_strength, 0.0f, 2.0f, 0.01f, true)));
    layout.prop(prop("Shininess", float_value(m.shininess, 1.0f, 256.0f, 1.0f, true)));
}

// ── Scene tab ───────────────────────────────────────────────────────

auto draw_lighting(sg::Light &headlight, Layout &layout) -> void {
    auto *light = &headlight;
    layout.prop(bool_prop("Enabled", light->enabled));

    auto dir = vec3_prop(
        "Direction", [light] { return light->direction; },
        [light](const sg::Vec3f &v) { light->direction = v; }, -1.0f, 1.0f,
        0.05f);
    dir.tooltip = "Camera space";
    layout.prop(std::move(dir));

    layout.prop(prop(
        "Color",
        ColorValue{ .get =
                        [light] {
                            const auto &c = light->color;
                            return sg::Vec4f{ c(0), c(1), c(2), 1.0f };
                        },
                    .set =
                        [light](const sg::Vec4f &c) {
                            light->color = sg::Vec3f(c.head<3>());
                        },
                    .alpha = false }));
}

// Adapt a draw function taking the required state by reference; poll()
// guarantees it exists.
auto on_object(void (*draw)(sg::Object &, Layout &)) {
    return [draw](const Context &c, Layout &l) { draw(*c.object, l); };
}
auto on_mesh(void (*draw)(sg::MeshData &, Layout &)) {
    return [draw](const Context &c, Layout &l) { draw(*c.mesh_data(), l); };
}
auto on_headlight(void (*draw)(sg::Light &, Layout &)) {
    return [draw](const Context &c, Layout &l) { draw(*c.headlight(), l); };
}

} // namespace

auto register_builtin_panels() -> void {
    static const bool once = [] {
        register_tab({ "object", "Object", "\u25A2", 0 });   // ▢
        register_tab({ "data", "Data", "\u25B3", 10 });      // △
        register_tab({ "material", "Material", "\u25CF", 20 }); // ●
        register_tab({ "scene", "Scene", "\u2600", 30 });    // ☀
        auto object = [](const Context &c) { return c.object != nullptr; };
        auto lit = [](const Context &c) {
            auto *md = c.mesh_data();
            return md && is_lit(*md);
        };
        register_panel({ "object.object", "object", 0, "Object", true, object, on_object(draw_object) });
        register_panel({ "object.transform", "object", 10, "Transform", true, object, on_object(draw_transform) });
        register_panel({ "data.render_layers", "data", 0, "Render Layers", true, has_mesh, on_mesh(draw_render_layers) });
        register_panel({ "data.shading", "data", 10, "Shading & Rendering", true, has_mesh, on_mesh(draw_shading) });
        register_panel({ "data.color", "data", 20, "Color", true, has_mesh, on_mesh(draw_color) });
        register_panel({ "data.attributes", "data", 30, "Attribute Bindings", false, has_mesh, on_mesh(draw_attributes) });
        register_panel({ "material.response", "material", 0, "Material Response", true, lit, on_mesh(draw_material) });
        register_panel({ "scene.lighting", "scene", 0, "Lighting", true,
                         [](const Context &c) { return c.headlight() != nullptr; },
                         on_headlight(draw_lighting) });
        return true;
    }();
    (void)once;
}

} // namespace balsa::visualization::controls
