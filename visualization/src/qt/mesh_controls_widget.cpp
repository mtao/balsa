#include "balsa/visualization/qt/mesh_controls_widget.hpp"
#include "balsa/visualization/qt/vertical_tab_widget.hpp"

// Qt defines 'emit' as a macro, which conflicts with TBB's
// tbb::profiling::emit().  Undefine it before pulling in headers
// that transitively include TBB (via quiver), then restore it.
#undef emit

#include "balsa/scene_graph/BVHData.hpp"
#include "balsa/scene_graph/Light.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/visualization/colormap_list.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"

// Restore Qt's emit macro (expands to nothing, but needed for readability).
#define emit

#include <QBoxLayout>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardItemModel>
#include <QString>

#include <algorithm>

namespace balsa::visualization::qt {

namespace vulkan = ::balsa::visualization::vulkan;
namespace sg = ::balsa::scene_graph;

using visualization::find_colormap_index;
using visualization::k_colormap_count;
using visualization::k_colormap_names;

// ── Helper: create a labeled slider ─────────────────────────────────

static QSlider *make_slider(int min, int max, int value, QWidget *parent) {
    auto *s = new QSlider(Qt::Horizontal, parent);
    s->setRange(min, max);
    s->setValue(value);
    return s;
}

// ── Helper: set QPushButton background to a color ───────────────────

static void
    set_button_color(QPushButton *btn, float r, float g, float b, float a) {
    QColor c = QColor::fromRgbF(r, g, b, a);
    btn->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid gray;")
            .arg(c.name(QColor::HexArgb)));
}

// ── Helper: make a labeled double spin box ──────────────────────────

static QDoubleSpinBox *make_spin(const char *label,
                                 QWidget *parent,
                                 QHBoxLayout *row,
                                 double min,
                                 double max,
                                 double step) {
    row->addWidget(new QLabel(label, parent));
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setDecimals(3);
    spin->setSingleStep(step);
    spin->setKeyboardTracking(false);
    row->addWidget(spin);
    return spin;
}

// ── Construction / Destruction ───────────────────────────────────────

MeshControlsWidget::MeshControlsWidget(QWidget *parent) : QWidget(parent) {
    build_ui();
}

MeshControlsWidget::~MeshControlsWidget() = default;

// ── set_scene / set_selected_object ─────────────────────────────────

void MeshControlsWidget::set_scene(
    ::balsa::visualization::vulkan::MeshScene *scene) {
    _scene = scene;
    _selected = nullptr;
    sync_from_state();
}

void MeshControlsWidget::set_selected_object(
    ::balsa::scene_graph::Object *obj) {
    _selected = obj;
    sync_from_state();
}

// ═════════════════════════════════════════════════════════════════════
// build_ui — top-level layout with VerticalTabWidget
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::build_ui() {
    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);

    _tabs = new VerticalTabWidget(this);

    // Unicode icon characters for each tab.
    // Using simple geometric / symbolic characters.
    _tab_object = _tabs->addTab(build_object_page(),
                                QStringLiteral("\u25A2"),
                                QStringLiteral("Object")); // ▢
    _tab_layers = _tabs->addTab(build_layers_page(),
                                QStringLiteral("\u2630"),
                                QStringLiteral("Layers")); // ☰
    _tab_material = _tabs->addTab(build_material_page(),
                                  QStringLiteral("\u25CF"),
                                  QStringLiteral("Material")); // ●
    _tab_bvh = _tabs->addTab(
        build_bvh_page(), QStringLiteral("\u2592"), QStringLiteral("BVH")); // ▒
    _tab_lighting = _tabs->addTab(build_lighting_page(),
                                  QStringLiteral("\u2600"),
                                  QStringLiteral("Lighting")); // ☀

    root_layout->addWidget(_tabs, 1);
}

// ═════════════════════════════════════════════════════════════════════
// Tab 1: Object
// ═════════════════════════════════════════════════════════════════════

QWidget *MeshControlsWidget::build_object_page() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    build_object_info(page, layout);
    build_transform_section(page, layout);

    layout->addStretch();
    return page;
}

void MeshControlsWidget::build_object_info(QWidget *parent,
                                           QBoxLayout *layout) {
    // Name
    auto *name_row = new QHBoxLayout;
    name_row->addWidget(new QLabel("Name:", parent));
    _name_edit = new QLineEdit(parent);
    name_row->addWidget(_name_edit);
    layout->addLayout(name_row);

    // Visibility
    _visible_check = new QCheckBox("Visible", parent);
    layout->addWidget(_visible_check);

    // Mesh info
    _vertex_count_label = new QLabel("Vertices: 0", parent);
    _triangle_count_label = new QLabel("Triangles: 0", parent);
    _edge_count_label = new QLabel("Edges: 0", parent);
    _attribute_label = new QLabel("Attributes: -", parent);
    layout->addWidget(_vertex_count_label);
    layout->addWidget(_triangle_count_label);
    layout->addWidget(_edge_count_label);
    layout->addWidget(_attribute_label);

    connect(_name_edit,
            &QLineEdit::textChanged,
            this,
            &MeshControlsWidget::on_name_edited);
    connect(_visible_check,
            &QCheckBox::toggled,
            this,
            &MeshControlsWidget::on_visibility_changed);
}

void MeshControlsWidget::build_transform_section(QWidget *parent,
                                                 QBoxLayout *layout) {
    _transform_group = new QGroupBox("Transform", parent);
    _transform_group->setEnabled(false);
    auto *grp_layout = new QVBoxLayout(_transform_group);

    // Translation
    auto *t_row = new QHBoxLayout;
    t_row->addWidget(new QLabel("T", _transform_group));
    _tx = make_spin("X", _transform_group, t_row, -1e6, 1e6, 0.1);
    _ty = make_spin("Y", _transform_group, t_row, -1e6, 1e6, 0.1);
    _tz = make_spin("Z", _transform_group, t_row, -1e6, 1e6, 0.1);
    grp_layout->addLayout(t_row);

    // Rotation (degrees)
    auto *r_row = new QHBoxLayout;
    r_row->addWidget(new QLabel("R", _transform_group));
    _rx = make_spin("X", _transform_group, r_row, -360.0, 360.0, 1.0);
    _ry = make_spin("Y", _transform_group, r_row, -360.0, 360.0, 1.0);
    _rz = make_spin("Z", _transform_group, r_row, -360.0, 360.0, 1.0);
    grp_layout->addLayout(r_row);

    // Scale
    auto *s_row = new QHBoxLayout;
    s_row->addWidget(new QLabel("S", _transform_group));
    _sx = make_spin("X", _transform_group, s_row, -1e6, 1e6, 0.1);
    _sy = make_spin("Y", _transform_group, s_row, -1e6, 1e6, 0.1);
    _sz = make_spin("Z", _transform_group, s_row, -1e6, 1e6, 0.1);
    _sx->setValue(1.0);
    _sy->setValue(1.0);
    _sz->setValue(1.0);
    grp_layout->addLayout(s_row);

    // Reset button
    _reset_transform_btn = new QPushButton("Reset Transform", _transform_group);
    grp_layout->addWidget(_reset_transform_btn);

    layout->addWidget(_transform_group);

    // Connect spin boxes
    for (auto *spin : {_tx, _ty, _tz}) {
        connect(spin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                &MeshControlsWidget::on_translation_changed);
    }
    for (auto *spin : {_rx, _ry, _rz}) {
        connect(spin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                &MeshControlsWidget::on_rotation_changed);
    }
    for (auto *spin : {_sx, _sy, _sz}) {
        connect(spin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                &MeshControlsWidget::on_scale_changed);
    }
    connect(_reset_transform_btn,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_reset_transform);
}

// ═════════════════════════════════════════════════════════════════════
// Tab 2: Layers
// ═════════════════════════════════════════════════════════════════════

QWidget *MeshControlsWidget::build_layers_page() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    build_layers_group(page, layout);
    build_render_state_group(page, layout);
    build_color_group(page, layout);
    build_attribute_bindings_group(page, layout);

    layout->addStretch();
    return page;
}

void MeshControlsWidget::build_layers_group(QWidget *parent,
                                            QBoxLayout *layout) {
    auto *group = new QGroupBox("Render Layers", parent);
    auto *grp_layout = new QVBoxLayout(group);

    // ── Solid sub-section ───────────────────────────────────────────
    auto *solid_row = new QHBoxLayout;
    _solid_enabled_check = new QCheckBox("Solid", group);
    solid_row->addWidget(_solid_enabled_check);
    _solid_color_button = new QPushButton(group);
    _solid_color_button->setVisible(
        false); // unused: solid color comes from Color section
    auto *solid_hint = new QLabel("(color: see Color section)", group);
    solid_hint->setStyleSheet("color: gray; font-size: 10px;");
    solid_row->addWidget(solid_hint);
    solid_row->addStretch();
    grp_layout->addLayout(solid_row);

    // ── Wireframe sub-section ───────────────────────────────────────
    auto *wire_row = new QHBoxLayout;
    _wireframe_enabled_check = new QCheckBox("Wireframe", group);
    wire_row->addWidget(_wireframe_enabled_check);
    _wireframe_color_button = new QPushButton(group);
    _wireframe_color_button->setFixedSize(60, 24);
    set_button_color(_wireframe_color_button, 0.0f, 0.0f, 0.0f, 1.0f);
    wire_row->addWidget(_wireframe_color_button);
    wire_row->addStretch();
    grp_layout->addLayout(wire_row);

    // Wireframe details (width slider) — visible only when enabled
    _wireframe_details_container = new QWidget(group);
    auto *wire_details_layout = new QHBoxLayout(_wireframe_details_container);
    wire_details_layout->setContentsMargins(20, 0, 0, 0); // indent
    _wireframe_width_label =
        new QLabel("Width: 1.5", _wireframe_details_container);
    wire_details_layout->addWidget(_wireframe_width_label);
    _wireframe_width_slider =
        make_slider(5, 50, 15, _wireframe_details_container); // *10 scale
    wire_details_layout->addWidget(_wireframe_width_slider);
    grp_layout->addWidget(_wireframe_details_container);

    // ── Points sub-section ──────────────────────────────────────────
    auto *point_row = new QHBoxLayout;
    _points_enabled_check = new QCheckBox("Points", group);
    point_row->addWidget(_points_enabled_check);
    _point_color_button = new QPushButton(group);
    _point_color_button->setFixedSize(60, 24);
    set_button_color(_point_color_button, 1.0f, 0.0f, 0.0f, 1.0f);
    point_row->addWidget(_point_color_button);
    point_row->addStretch();
    grp_layout->addLayout(point_row);

    // Point details (size slider) — visible only when enabled
    _points_details_container = new QWidget(group);
    auto *point_details_layout = new QHBoxLayout(_points_details_container);
    point_details_layout->setContentsMargins(20, 0, 0, 0); // indent
    _point_size_label = new QLabel("Size: 3.0", _points_details_container);
    point_details_layout->addWidget(_point_size_label);
    _point_size_slider =
        make_slider(1, 200, 30, _points_details_container); // *10 scale
    point_details_layout->addWidget(_point_size_slider);
    grp_layout->addWidget(_points_details_container);

    layout->addWidget(group);

    // Connections
    connect(_solid_enabled_check,
            &QCheckBox::toggled,
            this,
            &MeshControlsWidget::on_solid_enabled_changed);
    connect(_solid_color_button,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_solid_color_clicked);
    connect(_wireframe_enabled_check,
            &QCheckBox::toggled,
            this,
            &MeshControlsWidget::on_wireframe_enabled_changed);
    connect(_wireframe_color_button,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_wireframe_color_clicked);
    connect(_wireframe_width_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_wireframe_width_changed);
    connect(_points_enabled_check,
            &QCheckBox::toggled,
            this,
            &MeshControlsWidget::on_points_enabled_changed);
    connect(_point_color_button,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_point_color_clicked);
    connect(_point_size_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_point_size_changed);
}

void MeshControlsWidget::build_render_state_group(QWidget *parent,
                                                  QBoxLayout *layout) {
    auto *group = new QGroupBox("Shading && Rendering", parent);
    auto *grp_layout = new QVBoxLayout(group);

    // Normal source (always visible — this controls whether lighting is active)
    auto *normal_row = new QHBoxLayout;
    normal_row->addWidget(new QLabel("Normals:", group));
    _normal_source_combo = new QComboBox(group);
    _normal_source_combo->addItems(
        {"From Attribute", "Computed (Flat)", "None (Unlit)"});
    normal_row->addWidget(_normal_source_combo);
    grp_layout->addLayout(normal_row);

    // Shading-dependent controls: only visible when lighting is active
    _shading_details_container = new QWidget(group);
    auto *shading_layout = new QVBoxLayout(_shading_details_container);
    shading_layout->setContentsMargins(0, 0, 0, 0);

    // Shading model
    auto *shading_row = new QHBoxLayout;
    shading_row->addWidget(new QLabel("Shading:", _shading_details_container));
    _shading_combo = new QComboBox(_shading_details_container);
    _shading_combo->addItems({"Flat", "Gouraud", "Phong"});
    shading_row->addWidget(_shading_combo);
    shading_layout->addLayout(shading_row);

    // Two-sided
    _two_sided_check =
        new QCheckBox("Two-Sided Lighting", _shading_details_container);
    shading_layout->addWidget(_two_sided_check);

    grp_layout->addWidget(_shading_details_container);

    // Face culling (always visible — useful regardless of lighting)
    auto *cull_row = new QHBoxLayout;
    cull_row->addWidget(new QLabel("Face Culling:", group));
    _cull_mode_combo = new QComboBox(group);
    _cull_mode_combo->addItems(
        {"None (Show All)", "Back (Exterior)", "Front (Interior)"});
    cull_row->addWidget(_cull_mode_combo);
    grp_layout->addLayout(cull_row);

    layout->addWidget(group);

    connect(_normal_source_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_normal_source_changed);
    connect(_shading_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_shading_changed);
    connect(_two_sided_check,
            &QCheckBox::toggled,
            this,
            &MeshControlsWidget::on_two_sided_changed);
    connect(_cull_mode_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_cull_mode_changed);
}

void MeshControlsWidget::build_color_group(QWidget *parent,
                                           QBoxLayout *layout) {
    auto *group = new QGroupBox("Color", parent);
    auto *grp_layout = new QVBoxLayout(group);

    // Color source
    auto *src_row = new QHBoxLayout;
    src_row->addWidget(new QLabel("Source:", group));
    _color_source_combo = new QComboBox(group);
    _color_source_combo->addItems(
        {"Uniform Color", "Per-Vertex Color", "Scalar Field"});
    src_row->addWidget(_color_source_combo);
    grp_layout->addLayout(src_row);

    // Uniform color button (hidden when not Uniform)
    _uniform_color_container = new QWidget(group);
    auto *uc_layout = new QHBoxLayout(_uniform_color_container);
    uc_layout->setContentsMargins(0, 0, 0, 0);
    uc_layout->addWidget(new QLabel("Color:", _uniform_color_container));
    _uniform_color_button = new QPushButton(_uniform_color_container);
    _uniform_color_button->setFixedSize(60, 24);
    set_button_color(_uniform_color_button, 0.8f, 0.8f, 0.8f, 1.0f);
    uc_layout->addWidget(_uniform_color_button);
    uc_layout->addStretch();
    grp_layout->addWidget(_uniform_color_container);

    // Scalar field controls (hidden when not ScalarField)
    _scalar_field_container = new QWidget(group);
    auto *sf_layout = new QVBoxLayout(_scalar_field_container);
    sf_layout->setContentsMargins(0, 0, 0, 0);

    // Colormap combo
    auto *cmap_row = new QHBoxLayout;
    cmap_row->addWidget(new QLabel("Colormap:", _scalar_field_container));
    _colormap_combo = new QComboBox(_scalar_field_container);
    for (int i = 0; i < k_colormap_count; ++i) {
        _colormap_combo->addItem(k_colormap_names[i]);
    }
    cmap_row->addWidget(_colormap_combo);
    sf_layout->addLayout(cmap_row);

    // Custom colormap input
    auto *custom_row = new QHBoxLayout;
    custom_row->addWidget(new QLabel("Custom:", _scalar_field_container));
    _colormap_custom_edit = new QLineEdit(_scalar_field_container);
    _colormap_custom_edit->setPlaceholderText("Custom colormap name");
    custom_row->addWidget(_colormap_custom_edit);
    sf_layout->addLayout(custom_row);

    // Scalar range
    auto *min_row = new QHBoxLayout;
    min_row->addWidget(new QLabel("Min:", _scalar_field_container));
    _scalar_min_spin = new QDoubleSpinBox(_scalar_field_container);
    _scalar_min_spin->setRange(-1e6, 1e6);
    _scalar_min_spin->setDecimals(4);
    _scalar_min_spin->setSingleStep(0.01);
    min_row->addWidget(_scalar_min_spin);
    sf_layout->addLayout(min_row);

    auto *max_row = new QHBoxLayout;
    max_row->addWidget(new QLabel("Max:", _scalar_field_container));
    _scalar_max_spin = new QDoubleSpinBox(_scalar_field_container);
    _scalar_max_spin->setRange(-1e6, 1e6);
    _scalar_max_spin->setDecimals(4);
    _scalar_max_spin->setSingleStep(0.01);
    _scalar_max_spin->setValue(1.0);
    max_row->addWidget(_scalar_max_spin);
    sf_layout->addLayout(max_row);

    _scalar_range_reset_button =
        new QPushButton("Reset Range", _scalar_field_container);
    sf_layout->addWidget(_scalar_range_reset_button);

    grp_layout->addWidget(_scalar_field_container);

    layout->addWidget(group);

    connect(_color_source_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_color_source_changed);
    connect(_uniform_color_button,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_uniform_color_clicked);
    connect(_colormap_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_colormap_changed);
    connect(_colormap_custom_edit,
            &QLineEdit::editingFinished,
            this,
            &MeshControlsWidget::on_colormap_custom_edited);
    connect(_scalar_min_spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &MeshControlsWidget::on_scalar_min_changed);
    connect(_scalar_max_spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &MeshControlsWidget::on_scalar_max_changed);
    connect(_scalar_range_reset_button,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_scalar_range_reset);
}

// ═════════════════════════════════════════════════════════════════════
// Attribute Bindings group
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::build_attribute_bindings_group(QWidget *parent,
                                                        QBoxLayout *layout) {
    _attr_bindings_group = new QGroupBox("Attribute Bindings", parent);
    auto *grp_layout = new QVBoxLayout(_attr_bindings_group);

    // Position attribute combo
    auto *pos_row = new QHBoxLayout;
    pos_row->addWidget(new QLabel("Position:", _attr_bindings_group));
    _position_attr_combo = new QComboBox(_attr_bindings_group);
    pos_row->addWidget(_position_attr_combo);
    grp_layout->addLayout(pos_row);

    // Normal attribute combo
    auto *norm_row = new QHBoxLayout;
    norm_row->addWidget(new QLabel("Normal:", _attr_bindings_group));
    _normal_attr_combo = new QComboBox(_attr_bindings_group);
    norm_row->addWidget(_normal_attr_combo);
    grp_layout->addLayout(norm_row);

    // Scalar attribute combo
    auto *scalar_row = new QHBoxLayout;
    scalar_row->addWidget(new QLabel("Scalar:", _attr_bindings_group));
    _scalar_attr_combo = new QComboBox(_attr_bindings_group);
    scalar_row->addWidget(_scalar_attr_combo);
    grp_layout->addLayout(scalar_row);

    // Scalar component selector (hidden when scalar not bound or 1-component)
    _scalar_component_container = new QWidget(_attr_bindings_group);
    auto *comp_layout = new QHBoxLayout(_scalar_component_container);
    comp_layout->setContentsMargins(20, 0, 0, 0); // indent
    comp_layout->addWidget(
        new QLabel("Component:", _scalar_component_container));
    _scalar_component_combo = new QComboBox(_scalar_component_container);
    comp_layout->addWidget(_scalar_component_combo);
    grp_layout->addWidget(_scalar_component_container);
    _scalar_component_container->setVisible(false);

    layout->addWidget(_attr_bindings_group);

    // Connections
    connect(_position_attr_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_position_attr_changed);
    connect(_normal_attr_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_normal_attr_changed);
    connect(_scalar_attr_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_scalar_attr_changed);
    connect(_scalar_component_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_scalar_component_changed);
}

// ═════════════════════════════════════════════════════════════════════
// Tab 3: Material
// ═════════════════════════════════════════════════════════════════════

QWidget *MeshControlsWidget::build_material_page() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    build_material_group(page, layout);

    // Placeholder for future named material system
    auto *placeholder =
        new QLabel("Named materials coming in a future update.", page);
    placeholder->setStyleSheet(
        "color: gray; font-size: 10px; font-style: italic;");
    placeholder->setWordWrap(true);
    layout->addWidget(placeholder);

    layout->addStretch();
    return page;
}

void MeshControlsWidget::build_material_group(QWidget *parent,
                                              QBoxLayout *layout) {
    auto *group = new QGroupBox("Material Response", parent);
    group->setToolTip("How this mesh responds to scene light");
    auto *grp_layout = new QVBoxLayout(group);

    // Subtitle
    auto *subtitle = new QLabel("How this mesh responds to scene light", group);
    subtitle->setStyleSheet("color: gray; font-size: 10px;");
    grp_layout->addWidget(subtitle);

    // Ambient
    auto *amb_row = new QHBoxLayout;
    _material_ambient_label = new QLabel("Ambient: 0.15", group);
    _material_ambient_label->setToolTip(
        "Base light the mesh receives regardless of light direction");
    amb_row->addWidget(_material_ambient_label);
    _material_ambient_slider = make_slider(0, 100, 15, group);
    _material_ambient_slider->setToolTip(
        "Base light the mesh receives regardless of light direction");
    amb_row->addWidget(_material_ambient_slider);
    grp_layout->addLayout(amb_row);

    // Diffuse
    auto *diff_row = new QHBoxLayout;
    _material_diffuse_label = new QLabel("Diffuse: 1.00", group);
    _material_diffuse_label->setToolTip(
        "Intensity of the diffuse lighting response");
    diff_row->addWidget(_material_diffuse_label);
    _material_diffuse_slider = make_slider(0, 200, 100, group);
    _material_diffuse_slider->setToolTip(
        "Intensity of the diffuse lighting response");
    diff_row->addWidget(_material_diffuse_slider);
    grp_layout->addLayout(diff_row);

    // Specular
    auto *spec_row = new QHBoxLayout;
    _material_specular_label = new QLabel("Specular: 0.50", group);
    _material_specular_label->setToolTip("Intensity of the specular highlight");
    spec_row->addWidget(_material_specular_label);
    _material_specular_slider = make_slider(0, 200, 50, group);
    _material_specular_slider->setToolTip(
        "Intensity of the specular highlight");
    spec_row->addWidget(_material_specular_slider);
    grp_layout->addLayout(spec_row);

    // Shininess
    auto *shin_row = new QHBoxLayout;
    _material_shininess_label = new QLabel("Shininess: 32", group);
    _material_shininess_label->setToolTip(
        "Sharpness of the specular highlight (higher = tighter)");
    shin_row->addWidget(_material_shininess_label);
    _material_shininess_slider = make_slider(1, 256, 32, group);
    _material_shininess_slider->setToolTip(
        "Sharpness of the specular highlight (higher = tighter)");
    shin_row->addWidget(_material_shininess_slider);
    grp_layout->addLayout(shin_row);

    layout->addWidget(group);

    connect(_material_ambient_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_material_ambient_changed);
    connect(_material_diffuse_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_material_diffuse_changed);
    connect(_material_specular_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_material_specular_changed);
    connect(_material_shininess_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_material_shininess_changed);
}

// ═════════════════════════════════════════════════════════════════════
// Tab 4: BVH
// ═════════════════════════════════════════════════════════════════════

QWidget *MeshControlsWidget::build_bvh_page() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    build_bvh_group(page, layout);

    layout->addStretch();
    return page;
}

void MeshControlsWidget::build_bvh_group(QWidget *parent, QBoxLayout *layout) {
    auto *group = new QGroupBox("BVH Overlay", parent);
    auto *grp_layout = new QVBoxLayout(group);

    // Enabled checkbox
    _bvh_enabled_check = new QCheckBox("Show Overlay", group);
    _bvh_enabled_check->setChecked(true);
    grp_layout->addWidget(_bvh_enabled_check);

    // KDOP type
    auto *kdop_row = new QHBoxLayout;
    kdop_row->addWidget(new QLabel("Bounding Volume:", group));
    _bvh_kdop_combo = new QComboBox(group);
    _bvh_kdop_combo->addItems({"AABB (K=3)", "9-DOP", "13-DOP"});
    kdop_row->addWidget(_bvh_kdop_combo);
    grp_layout->addLayout(kdop_row);

    // Build strategy
    auto *strat_row = new QHBoxLayout;
    strat_row->addWidget(new QLabel("Strategy:", group));
    _bvh_strategy_combo = new QComboBox(group);
    _bvh_strategy_combo->addItems({"SAH (Best Quality)",
                                   "Object Median (Balanced)",
                                   "Spatial Median",
                                   "LBVH (Fastest)"});
    strat_row->addWidget(_bvh_strategy_combo);
    grp_layout->addLayout(strat_row);

    // Max leaf size
    auto *leaf_row = new QHBoxLayout;
    _bvh_leaf_size_label = new QLabel("Max Leaf: 4", group);
    leaf_row->addWidget(_bvh_leaf_size_label);
    _bvh_leaf_size_slider = make_slider(1, 32, 4, group);
    leaf_row->addWidget(_bvh_leaf_size_slider);
    grp_layout->addLayout(leaf_row);

    // BVH height info
    _bvh_height_label = new QLabel("BVH height: -", group);
    _bvh_height_label->setStyleSheet("color: gray; font-size: 10px;");
    grp_layout->addWidget(_bvh_height_label);

    // Display depth
    auto *depth_row = new QHBoxLayout;
    _bvh_depth_label = new QLabel("Depth: 0", group);
    depth_row->addWidget(_bvh_depth_label);
    _bvh_depth_slider = make_slider(0, 20, 0, group);
    depth_row->addWidget(_bvh_depth_slider);
    grp_layout->addLayout(depth_row);

    // Color
    auto *color_row = new QHBoxLayout;
    color_row->addWidget(new QLabel("Color:", group));
    _bvh_color_button = new QPushButton(group);
    _bvh_color_button->setFixedSize(60, 24);
    set_button_color(_bvh_color_button, 0.1f, 0.8f, 0.2f, 1.0f);
    color_row->addWidget(_bvh_color_button);
    color_row->addStretch();
    grp_layout->addLayout(color_row);

    layout->addWidget(group);

    connect(_bvh_enabled_check,
            &QCheckBox::toggled,
            this,
            &MeshControlsWidget::on_bvh_enabled_changed);
    connect(_bvh_kdop_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_bvh_kdop_changed);
    connect(_bvh_strategy_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MeshControlsWidget::on_bvh_strategy_changed);
    connect(_bvh_leaf_size_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_bvh_leaf_size_changed);
    connect(_bvh_depth_slider,
            &QSlider::valueChanged,
            this,
            &MeshControlsWidget::on_bvh_depth_changed);
    connect(_bvh_color_button,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_bvh_color_clicked);
}

// ═════════════════════════════════════════════════════════════════════
// Tab 5: Lighting
// ═════════════════════════════════════════════════════════════════════

QWidget *MeshControlsWidget::build_lighting_page() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    build_scene_lighting_group(page, layout);

    layout->addStretch();
    return page;
}

void MeshControlsWidget::build_scene_lighting_group(QWidget *parent,
                                                    QBoxLayout *layout) {
    auto *group = new QGroupBox("Scene Lighting", parent);
    auto *grp_layout = new QVBoxLayout(group);

    // Subtitle
    auto *subtitle = new QLabel("Global light affecting all meshes", group);
    subtitle->setStyleSheet("color: gray; font-size: 10px;");
    grp_layout->addWidget(subtitle);

    // Enabled checkbox
    _scene_light_enabled_check = new QCheckBox("Enabled", group);
    _scene_light_enabled_check->setChecked(true);
    grp_layout->addWidget(_scene_light_enabled_check);

    // Direction (camera-local)
    auto *dir_layout = new QHBoxLayout;
    dir_layout->addWidget(new QLabel("Dir (cam):", group));
    _scene_light_x_spin = new QDoubleSpinBox(group);
    _scene_light_y_spin = new QDoubleSpinBox(group);
    _scene_light_z_spin = new QDoubleSpinBox(group);
    for (auto *s :
         {_scene_light_x_spin, _scene_light_y_spin, _scene_light_z_spin}) {
        s->setRange(-1.0, 1.0);
        s->setDecimals(3);
        s->setSingleStep(0.01);
        dir_layout->addWidget(s);
    }
    _scene_light_x_spin->setValue(0.0);
    _scene_light_y_spin->setValue(0.0);
    _scene_light_z_spin->setValue(1.0);
    grp_layout->addLayout(dir_layout);

    // Light color
    auto *color_row = new QHBoxLayout;
    color_row->addWidget(new QLabel("Color:", group));
    _scene_light_color_button = new QPushButton(group);
    _scene_light_color_button->setFixedSize(60, 24);
    set_button_color(_scene_light_color_button, 1.0f, 1.0f, 1.0f, 1.0f);
    color_row->addWidget(_scene_light_color_button);
    color_row->addStretch();
    grp_layout->addLayout(color_row);

    layout->addWidget(group);

    connect(_scene_light_enabled_check,
            &QCheckBox::toggled,
            this,
            &MeshControlsWidget::on_scene_light_enabled_changed);
    connect(_scene_light_x_spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &MeshControlsWidget::on_scene_light_dir_changed);
    connect(_scene_light_y_spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &MeshControlsWidget::on_scene_light_dir_changed);
    connect(_scene_light_z_spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &MeshControlsWidget::on_scene_light_dir_changed);
    connect(_scene_light_color_button,
            &QPushButton::clicked,
            this,
            &MeshControlsWidget::on_scene_light_color_clicked);
}

// ═════════════════════════════════════════════════════════════════════
// selected_mesh_data / selected_bvh_data
// ═════════════════════════════════════════════════════════════════════

sg::MeshData *MeshControlsWidget::selected_mesh_data() {
    if (!_selected) return nullptr;
    return _selected->find_feature<sg::MeshData>();
}

sg::BVHData *MeshControlsWidget::selected_bvh_data() {
    if (!_selected) return nullptr;
    return _selected->find_feature<sg::BVHData>();
}

// ═════════════════════════════════════════════════════════════════════
// sync_from_state
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::sync_from_state() {
    auto *obj = _selected;
    auto *mesh_data = selected_mesh_data();
    bool have_selection = (obj != nullptr);
    bool have_mesh = (have_selection && mesh_data != nullptr);

    // Show/hide tabs based on selection state.
    // Object tab always visible (shows "no selection" state gracefully).
    // Layers/Material tabs only when a mesh is selected.
    // BVH tab only when BVH data is present.
    // Lighting tab always visible (scene-global).
    _tabs->setTabVisible(_tab_layers, have_mesh);

    // Apply constraints before syncing UI — ensures widgets reflect
    // valid state even if the render state was mutated externally.
    bool mesh_has_normals = have_mesh && mesh_data->has_normals();
    if (have_mesh) { mesh_data->render_state().constrain(mesh_has_normals); }

    // Material tab is only visible when lighting is active.
    bool is_lit = have_mesh
                  && mesh_data->render_state().normal_source
                         != vulkan::NormalSource::None;
    _tabs->setTabVisible(_tab_material, is_lit);
    if (_shading_details_container) {
        _shading_details_container->setVisible(is_lit);
    }

    // BVH tab
    auto *bvh_data =
        have_selection ? obj->find_feature<sg::BVHData>() : nullptr;
    _tabs->setTabVisible(_tab_bvh, bvh_data != nullptr);

    // Mesh info labels visibility
    _vertex_count_label->setVisible(have_mesh);
    _triangle_count_label->setVisible(have_mesh);
    _edge_count_label->setVisible(have_mesh);
    _attribute_label->setVisible(have_mesh);

    // Transform group
    _transform_group->setEnabled(have_selection);

    if (!obj) return;

    // Block signals to avoid feedback loops
    QSignalBlocker b1(_name_edit);
    QSignalBlocker b2(_visible_check);
    QSignalBlocker b3(_shading_combo);
    QSignalBlocker b4(_normal_source_combo);
    QSignalBlocker b5(_two_sided_check);
    QSignalBlocker b5b(_cull_mode_combo);
    QSignalBlocker b6(_color_source_combo);
    QSignalBlocker b7(_colormap_combo);
    QSignalBlocker b8(_colormap_custom_edit);
    QSignalBlocker b9(_scalar_min_spin);
    QSignalBlocker b10(_scalar_max_spin);
    QSignalBlocker b11(_solid_enabled_check);
    QSignalBlocker b12(_wireframe_enabled_check);
    QSignalBlocker b13(_wireframe_width_slider);
    QSignalBlocker b14(_points_enabled_check);
    QSignalBlocker b15(_point_size_slider);
    QSignalBlocker b16(_material_ambient_slider);
    QSignalBlocker b16b(_material_diffuse_slider);
    QSignalBlocker b17(_material_specular_slider);
    QSignalBlocker b18(_material_shininess_slider);
    QSignalBlocker b19(_scene_light_enabled_check);
    QSignalBlocker b20(_scene_light_x_spin);
    QSignalBlocker b21(_scene_light_y_spin);
    QSignalBlocker b22(_scene_light_z_spin);

    // Object info
    _name_edit->setText(QString::fromStdString(obj->name));
    _visible_check->setChecked(obj->visible);

    // Sync transform
    sync_transform_from_object();

    if (mesh_data) {
        _vertex_count_label->setText(
            QStringLiteral("Vertices: %1")
                .arg(static_cast<quint64>(mesh_data->vertex_count())));
        _triangle_count_label->setText(
            QStringLiteral("Triangles: %1")
                .arg(static_cast<quint64>(mesh_data->triangle_count())));
        _edge_count_label->setText(
            QStringLiteral("Edges: %1")
                .arg(static_cast<quint64>(mesh_data->edge_count())));

        QString attrs;
        if (mesh_data->has_positions()) attrs += "P ";
        if (mesh_data->has_normals()) attrs += "N ";
        if (mesh_data->has_scalar_field()) attrs += "S ";
        _attribute_label->setText(
            QStringLiteral("Attributes: %1")
                .arg(attrs.isEmpty() ? "-" : attrs.trimmed()));

        const auto &s = mesh_data->render_state();

        // Render state
        _shading_combo->setCurrentIndex(static_cast<int>(s.shading));
        _normal_source_combo->setCurrentIndex(
            static_cast<int>(s.normal_source));
        _two_sided_check->setChecked(s.two_sided);
        _cull_mode_combo->setCurrentIndex(static_cast<int>(s.cull_mode));

        // Disable combo items that violate constraints.
        // "From Attribute" (index 0) requires actual normal data.
        if (auto *model = qobject_cast<QStandardItemModel *>(
                _normal_source_combo->model())) {
            auto *item0 = model->item(0);
            if (item0) { item0->setEnabled(mesh_has_normals); }
        }
        // "Gouraud" (1) and "Phong" (2) require FromAttribute normals.
        if (auto *model =
                qobject_cast<QStandardItemModel *>(_shading_combo->model())) {
            bool can_smooth =
                (s.normal_source == vulkan::NormalSource::FromAttribute);
            for (int i = 1; i <= 2; ++i) {
                auto *item = model->item(i);
                if (item) item->setEnabled(can_smooth);
            }
        }

        // Color
        _color_source_combo->setCurrentIndex(static_cast<int>(s.color_source));
        set_button_color(_uniform_color_button,
                         s.uniform_color[0],
                         s.uniform_color[1],
                         s.uniform_color[2],
                         s.uniform_color[3]);

        int cmap_idx = find_colormap_index(s.colormap_name);
        if (cmap_idx >= 0) { _colormap_combo->setCurrentIndex(cmap_idx); }
        _colormap_custom_edit->setText(QString::fromStdString(s.colormap_name));
        _scalar_min_spin->setValue(static_cast<double>(s.scalar_min));
        _scalar_max_spin->setValue(static_cast<double>(s.scalar_max));

        sync_color_group_visibility();

        // Attribute bindings
        sync_attribute_bindings();

        // Render layers
        const auto &layers = s.layers;

        _solid_enabled_check->setChecked(layers.solid.enabled);
        set_button_color(_solid_color_button,
                         layers.solid.color[0],
                         layers.solid.color[1],
                         layers.solid.color[2],
                         layers.solid.color[3]);
        _solid_color_button->setVisible(layers.solid.enabled);

        _wireframe_enabled_check->setChecked(layers.wireframe.enabled);
        set_button_color(_wireframe_color_button,
                         layers.wireframe.color[0],
                         layers.wireframe.color[1],
                         layers.wireframe.color[2],
                         layers.wireframe.color[3]);
        _wireframe_color_button->setVisible(layers.wireframe.enabled);
        _wireframe_details_container->setVisible(layers.wireframe.enabled);
        _wireframe_width_slider->setValue(
            static_cast<int>(layers.wireframe.width * 10.0f));
        _wireframe_width_label->setText(
            QStringLiteral("Width: %1").arg(layers.wireframe.width, 0, 'f', 1));

        _points_enabled_check->setChecked(layers.points.enabled);
        set_button_color(_point_color_button,
                         layers.points.color[0],
                         layers.points.color[1],
                         layers.points.color[2],
                         layers.points.color[3]);
        _point_color_button->setVisible(layers.points.enabled);
        _points_details_container->setVisible(layers.points.enabled);
        _point_size_slider->setValue(
            static_cast<int>(layers.points.size * 10.0f));
        _point_size_label->setText(
            QStringLiteral("Size: %1").arg(layers.points.size, 0, 'f', 1));

        // Material
        const auto &mat = s.material;
        _material_ambient_slider->setValue(
            static_cast<int>(mat.ambient_strength * 100.0f));
        _material_diffuse_slider->setValue(
            static_cast<int>(mat.diffuse_strength * 100.0f));
        _material_specular_slider->setValue(
            static_cast<int>(mat.specular_strength * 100.0f));
        _material_shininess_slider->setValue(static_cast<int>(mat.shininess));
        _material_ambient_label->setText(
            QStringLiteral("Ambient: %1").arg(mat.ambient_strength, 0, 'f', 2));
        _material_diffuse_label->setText(
            QStringLiteral("Diffuse: %1").arg(mat.diffuse_strength, 0, 'f', 2));
        _material_specular_label->setText(
            QStringLiteral("Specular: %1")
                .arg(mat.specular_strength, 0, 'f', 2));
        _material_shininess_label->setText(
            QStringLiteral("Shininess: %1")
                .arg(static_cast<int>(mat.shininess)));
    }

    // Scene lighting (always synced, not per-mesh)
    if (_scene) {
        auto &light = _scene->headlight();
        _scene_light_enabled_check->setChecked(light.enabled);
        _scene_light_x_spin->setValue(static_cast<double>(light.direction(0)));
        _scene_light_y_spin->setValue(static_cast<double>(light.direction(1)));
        _scene_light_z_spin->setValue(static_cast<double>(light.direction(2)));
        set_button_color(_scene_light_color_button,
                         static_cast<float>(light.color(0)),
                         static_cast<float>(light.color(1)),
                         static_cast<float>(light.color(2)),
                         1.0f);
    }

    // BVH overlay
    if (bvh_data) {
        QSignalBlocker bb1(_bvh_enabled_check);
        QSignalBlocker bb2(_bvh_kdop_combo);
        QSignalBlocker bb3(_bvh_strategy_combo);
        QSignalBlocker bb4(_bvh_leaf_size_slider);
        QSignalBlocker bb5(_bvh_depth_slider);

        _bvh_enabled_check->setChecked(bvh_data->enabled);

        // KDOP combo: 0=K3, 1=K9, 2=K13
        int kdop_idx = (bvh_data->kdop_k == 3)   ? 0
                       : (bvh_data->kdop_k == 9) ? 1
                                                 : 2;
        _bvh_kdop_combo->setCurrentIndex(kdop_idx);

        _bvh_strategy_combo->setCurrentIndex(
            static_cast<int>(bvh_data->strategy));

        _bvh_leaf_size_slider->setValue(
            static_cast<int>(bvh_data->max_leaf_size));
        _bvh_leaf_size_label->setText(
            QStringLiteral("Max Leaf: %1").arg(bvh_data->max_leaf_size));

        if (bvh_data->is_built()) {
            _bvh_height_label->setText(
                QStringLiteral("BVH height: %1").arg(bvh_data->bvh_height()));
            _bvh_depth_slider->setRange(0, bvh_data->bvh_height());
        } else {
            _bvh_height_label->setText("BVH height: -");
            _bvh_depth_slider->setRange(0, 20);
        }
        _bvh_depth_slider->setValue(bvh_data->display_depth);
        _bvh_depth_label->setText(
            QStringLiteral("Depth: %1").arg(bvh_data->display_depth));

        set_button_color(_bvh_color_button,
                         bvh_data->color[0],
                         bvh_data->color[1],
                         bvh_data->color[2],
                         bvh_data->color[3]);
    }
}

void MeshControlsWidget::sync_color_group_visibility() {
    auto *mesh_data = selected_mesh_data();
    if (!mesh_data) return;
    auto src = mesh_data->render_state().color_source;
    _uniform_color_container->setVisible(src == vulkan::ColorSource::Uniform);
    _scalar_field_container->setVisible(src
                                        == vulkan::ColorSource::ScalarField);
}

void MeshControlsWidget::sync_attribute_bindings() {
    auto *mesh_data = selected_mesh_data();
    bool have_mesh = (mesh_data != nullptr);

    _attr_bindings_group->setVisible(have_mesh);
    if (!have_mesh) return;

    const auto &discovered = mesh_data->discovered_attributes();

    // Block signals during repopulation
    QSignalBlocker bp(_position_attr_combo);
    QSignalBlocker bn(_normal_attr_combo);
    QSignalBlocker bs(_scalar_attr_combo);
    QSignalBlocker bc(_scalar_component_combo);

    // ── Helper: populate a combo with filtered discovered attributes ──
    // Returns the combo index of the currently-bound attribute, or 0 (None).
    auto populate_combo = [&](QComboBox *combo,
                              const sg::RoleBinding &binding,
                              auto filter_fn) -> int {
        combo->clear();
        combo->addItem("(None)"); // index 0 = unbound

        int current_idx = 0;
        for (int i = 0; i < static_cast<int>(discovered.size()); ++i) {
            if (!filter_fn(discovered[i])) continue;

            QString label =
                QStringLiteral("%1 (dim%2, %3 comp, %4 elts)")
                    .arg(QString::fromStdString(discovered[i].name))
                    .arg(static_cast<int>(discovered[i].dimension))
                    .arg(static_cast<unsigned>(discovered[i].component_count))
                    .arg(static_cast<quint64>(discovered[i].count));
            combo->addItem(label, QVariant(i)); // userData = discovered index

            if (binding.is_bound()
                && &binding.source.attribute()
                       == &discovered[i].handle.attribute()) {
                current_idx = combo->count() - 1;
            }
        }
        return current_idx;
    };

    // Position: floating-point, 1+ components
    int pos_idx = populate_combo(_position_attr_combo,
                                 mesh_data->position_binding(),
                                 [](const sg::DiscoveredAttribute &da) {
                                     return da.is_floating_point
                                            && da.component_count >= 1;
                                 });
    _position_attr_combo->setCurrentIndex(pos_idx);

    // Normal: floating-point, 2+ components
    int norm_idx = populate_combo(_normal_attr_combo,
                                  mesh_data->normal_binding(),
                                  [](const sg::DiscoveredAttribute &da) {
                                      return da.is_floating_point
                                             && da.component_count >= 2;
                                  });
    _normal_attr_combo->setCurrentIndex(norm_idx);

    // Scalar: any attribute with 1+ components
    int scl_idx = populate_combo(_scalar_attr_combo,
                                 mesh_data->scalar_binding(),
                                 [](const sg::DiscoveredAttribute &da) {
                                     return da.component_count >= 1;
                                 });
    _scalar_attr_combo->setCurrentIndex(scl_idx);

    // Scalar component selector
    bool show_component = false;
    if (mesh_data->has_scalar_field()) {
        const auto &scl_bind = mesh_data->scalar_binding();
        uint8_t src_components = 0;
        for (const auto &da : discovered) {
            if (scl_bind.is_bound()
                && &scl_bind.source.attribute() == &da.handle.attribute()) {
                src_components = da.component_count;
                break;
            }
        }
        if (src_components > 1) {
            show_component = true;
            _scalar_component_combo->clear();
            _scalar_component_combo->addItem("Magnitude"); // index 0 -> comp -1
            static const char *comp_names[] = {
                "X (0)", "Y (1)", "Z (2)", "W (3)"};
            int max_comp = std::min(static_cast<int>(src_components), 4);
            for (int c = 0; c < max_comp; ++c) {
                _scalar_component_combo->addItem(
                    comp_names[c]); // index c+1 -> comp c
            }
            // Map current component: -1 -> 0, 0 -> 1, 1 -> 2, ...
            int combo_idx = mesh_data->scalar_component() + 1;
            int max_items = max_comp + 1;
            if (combo_idx < 0 || combo_idx >= max_items) combo_idx = 0;
            _scalar_component_combo->setCurrentIndex(combo_idx);
        }
    }
    _scalar_component_container->setVisible(show_component);
}

void MeshControlsWidget::sync_transform_from_object() {
    auto *obj = _selected;
    _transform_group->setEnabled(obj != nullptr);

    if (!obj) return;

    QSignalBlocker b1(_tx), b2(_ty), b3(_tz);
    QSignalBlocker b4(_rx), b5(_ry), b6(_rz);
    QSignalBlocker b7(_sx), b8(_sy), b9(_sz);

    auto &t = obj->translation();
    _tx->setValue(static_cast<double>(static_cast<float>(t(0))));
    _ty->setValue(static_cast<double>(static_cast<float>(t(1))));
    _tz->setValue(static_cast<double>(static_cast<float>(t(2))));

    auto euler = obj->rotation_euler();
    _rx->setValue(static_cast<double>(static_cast<float>(euler(0))));
    _ry->setValue(static_cast<double>(static_cast<float>(euler(1))));
    _rz->setValue(static_cast<double>(static_cast<float>(euler(2))));

    auto &s = obj->scale_factors();
    _sx->setValue(static_cast<double>(static_cast<float>(s(0))));
    _sy->setValue(static_cast<double>(static_cast<float>(s(1))));
    _sz->setValue(static_cast<double>(static_cast<float>(s(2))));
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Visibility
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_visibility_changed(bool checked) {
    if (!_selected) return;
    _selected->visible = checked;
    emit scene_changed();
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Render state
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_shading_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().shading = static_cast<vulkan::ShadingModel>(index);
    md->render_state().constrain(md->has_normals());
    sync_from_state();
    emit scene_changed();
}

void MeshControlsWidget::on_color_source_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().color_source = static_cast<vulkan::ColorSource>(index);
    sync_color_group_visibility();
    emit scene_changed();
}

void MeshControlsWidget::on_normal_source_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().normal_source = static_cast<vulkan::NormalSource>(index);
    md->render_state().constrain(md->has_normals());

    // Re-sync UI to reflect any constraint-driven changes (e.g.
    // shading downgraded from Phong to Flat) and visibility updates.
    sync_from_state();

    emit scene_changed();
}

void MeshControlsWidget::on_two_sided_changed(bool checked) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().two_sided = checked;
    emit scene_changed();
}

void MeshControlsWidget::on_cull_mode_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().cull_mode = static_cast<vulkan::CullMode>(index);
    emit scene_changed();
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Color
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_uniform_color_clicked() {
    auto *md = selected_mesh_data();
    if (!md) return;
    auto &uc = md->render_state().uniform_color;
    QColor initial = QColor::fromRgbF(uc[0], uc[1], uc[2], uc[3]);
    QColor chosen = QColorDialog::getColor(
        initial, this, "Uniform Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        uc[0] = static_cast<float>(chosen.redF());
        uc[1] = static_cast<float>(chosen.greenF());
        uc[2] = static_cast<float>(chosen.blueF());
        uc[3] = static_cast<float>(chosen.alphaF());
        set_button_color(_uniform_color_button, uc[0], uc[1], uc[2], uc[3]);
        emit scene_changed();
    }
}

void MeshControlsWidget::on_colormap_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md || index < 0 || index >= k_colormap_count) return;
    md->render_state().colormap_name = k_colormap_names[index];
    {
        QSignalBlocker block(_colormap_custom_edit);
        _colormap_custom_edit->setText(
            QString::fromStdString(md->render_state().colormap_name));
    }
    emit scene_changed();
}

void MeshControlsWidget::on_colormap_custom_edited() {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().colormap_name =
        _colormap_custom_edit->text().toStdString();
    // Try to sync the combo to match
    int idx = find_colormap_index(md->render_state().colormap_name);
    if (idx >= 0) {
        QSignalBlocker block(_colormap_combo);
        _colormap_combo->setCurrentIndex(idx);
    }
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_min_changed(double value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().scalar_min = static_cast<float>(value);
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_max_changed(double value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().scalar_max = static_cast<float>(value);
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_range_reset() {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().scalar_min = 0.0f;
    md->render_state().scalar_max = 1.0f;
    {
        QSignalBlocker b1(_scalar_min_spin);
        QSignalBlocker b2(_scalar_max_spin);
        _scalar_min_spin->setValue(0.0);
        _scalar_max_spin->setValue(1.0);
    }
    emit scene_changed();
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Attribute bindings
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_position_attr_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md) return;

    if (index <= 0) {
        md->clear_position();
    } else {
        QVariant data = _position_attr_combo->itemData(index);
        if (data.isValid()) {
            int disc_idx = data.toInt();
            const auto &discovered = md->discovered_attributes();
            if (disc_idx >= 0
                && disc_idx < static_cast<int>(discovered.size())) {
                md->assign_position(discovered[disc_idx].handle);
            }
        }
    }
    sync_from_state();
    emit scene_changed();
}

void MeshControlsWidget::on_normal_attr_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md) return;

    if (index <= 0) {
        md->clear_normal();
    } else {
        QVariant data = _normal_attr_combo->itemData(index);
        if (data.isValid()) {
            int disc_idx = data.toInt();
            const auto &discovered = md->discovered_attributes();
            if (disc_idx >= 0
                && disc_idx < static_cast<int>(discovered.size())) {
                md->assign_normal(discovered[disc_idx].handle);
            }
        }
    }
    sync_from_state();
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_attr_changed(int index) {
    auto *md = selected_mesh_data();
    if (!md) return;

    if (index <= 0) {
        md->clear_scalar();
    } else {
        QVariant data = _scalar_attr_combo->itemData(index);
        if (data.isValid()) {
            int disc_idx = data.toInt();
            const auto &discovered = md->discovered_attributes();
            if (disc_idx >= 0
                && disc_idx < static_cast<int>(discovered.size())) {
                md->assign_scalar(discovered[disc_idx].handle,
                                  md->scalar_component());
            }
        }
    }
    sync_from_state();
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_component_changed(int combo_idx) {
    auto *md = selected_mesh_data();
    if (!md || !md->has_scalar_field()) return;

    // Map combo index: 0 -> -1 (Magnitude), 1 -> 0 (X), 2 -> 1 (Y), ...
    int new_comp = combo_idx - 1;
    md->assign_scalar(md->scalar_binding().source, new_comp);
    emit scene_changed();
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Render layers
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_solid_enabled_changed(bool checked) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().layers.solid.enabled = checked;
    _solid_color_button->setVisible(checked);
    emit scene_changed();
}

void MeshControlsWidget::on_solid_color_clicked() {
    auto *md = selected_mesh_data();
    if (!md) return;
    auto &c = md->render_state().layers.solid.color;
    QColor initial = QColor::fromRgbF(c[0], c[1], c[2], c[3]);
    QColor chosen = QColorDialog::getColor(
        initial, this, "Solid Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        c[0] = static_cast<float>(chosen.redF());
        c[1] = static_cast<float>(chosen.greenF());
        c[2] = static_cast<float>(chosen.blueF());
        c[3] = static_cast<float>(chosen.alphaF());
        set_button_color(_solid_color_button, c[0], c[1], c[2], c[3]);
        emit scene_changed();
    }
}

void MeshControlsWidget::on_wireframe_enabled_changed(bool checked) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().layers.wireframe.enabled = checked;
    _wireframe_color_button->setVisible(checked);
    _wireframe_details_container->setVisible(checked);
    emit scene_changed();
}

void MeshControlsWidget::on_wireframe_color_clicked() {
    auto *md = selected_mesh_data();
    if (!md) return;
    auto &c = md->render_state().layers.wireframe.color;
    QColor initial = QColor::fromRgbF(c[0], c[1], c[2], c[3]);
    QColor chosen = QColorDialog::getColor(
        initial, this, "Wireframe Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        c[0] = static_cast<float>(chosen.redF());
        c[1] = static_cast<float>(chosen.greenF());
        c[2] = static_cast<float>(chosen.blueF());
        c[3] = static_cast<float>(chosen.alphaF());
        set_button_color(_wireframe_color_button, c[0], c[1], c[2], c[3]);
        emit scene_changed();
    }
}

void MeshControlsWidget::on_wireframe_width_changed(int value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    float width = static_cast<float>(value) / 10.0f;
    md->render_state().layers.wireframe.width = width;
    _wireframe_width_label->setText(
        QStringLiteral("Width: %1").arg(width, 0, 'f', 1));
    emit scene_changed();
}

void MeshControlsWidget::on_points_enabled_changed(bool checked) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().layers.points.enabled = checked;
    _point_color_button->setVisible(checked);
    _points_details_container->setVisible(checked);
    emit scene_changed();
}

void MeshControlsWidget::on_point_color_clicked() {
    auto *md = selected_mesh_data();
    if (!md) return;
    auto &c = md->render_state().layers.points.color;
    QColor initial = QColor::fromRgbF(c[0], c[1], c[2], c[3]);
    QColor chosen = QColorDialog::getColor(
        initial, this, "Point Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        c[0] = static_cast<float>(chosen.redF());
        c[1] = static_cast<float>(chosen.greenF());
        c[2] = static_cast<float>(chosen.blueF());
        c[3] = static_cast<float>(chosen.alphaF());
        set_button_color(_point_color_button, c[0], c[1], c[2], c[3]);
        emit scene_changed();
    }
}

void MeshControlsWidget::on_point_size_changed(int value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    float size = static_cast<float>(value) / 10.0f;
    md->render_state().layers.points.size = size;
    _point_size_label->setText(QStringLiteral("Size: %1").arg(size, 0, 'f', 1));
    emit scene_changed();
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Material
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_material_ambient_changed(int value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    float v = static_cast<float>(value) / 100.0f;
    md->render_state().material.ambient_strength = v;
    _material_ambient_label->setText(
        QStringLiteral("Ambient: %1").arg(v, 0, 'f', 2));
    emit scene_changed();
}

void MeshControlsWidget::on_material_specular_changed(int value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    float v = static_cast<float>(value) / 100.0f;
    md->render_state().material.specular_strength = v;
    _material_specular_label->setText(
        QStringLiteral("Specular: %1").arg(v, 0, 'f', 2));
    emit scene_changed();
}

void MeshControlsWidget::on_material_shininess_changed(int value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    md->render_state().material.shininess = static_cast<float>(value);
    _material_shininess_label->setText(
        QStringLiteral("Shininess: %1").arg(value));
    emit scene_changed();
}

void MeshControlsWidget::on_material_diffuse_changed(int value) {
    auto *md = selected_mesh_data();
    if (!md) return;
    float v = static_cast<float>(value) / 100.0f;
    md->render_state().material.diffuse_strength = v;
    _material_diffuse_label->setText(
        QStringLiteral("Diffuse: %1").arg(v, 0, 'f', 2));
    emit scene_changed();
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Scene lighting
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_scene_light_enabled_changed(bool checked) {
    if (!_scene) return;
    _scene->headlight().enabled = checked;
    emit scene_changed();
}

void MeshControlsWidget::on_scene_light_dir_changed() {
    if (!_scene) return;
    auto &light = _scene->headlight();
    light.direction(0) = static_cast<float>(_scene_light_x_spin->value());
    light.direction(1) = static_cast<float>(_scene_light_y_spin->value());
    light.direction(2) = static_cast<float>(_scene_light_z_spin->value());
    emit scene_changed();
}

void MeshControlsWidget::on_scene_light_color_clicked() {
    if (!_scene) return;
    auto &light = _scene->headlight();
    QColor initial = QColor::fromRgbF(static_cast<qreal>(light.color(0)),
                                      static_cast<qreal>(light.color(1)),
                                      static_cast<qreal>(light.color(2)));
    QColor chosen = QColorDialog::getColor(initial, this, "Light Color");
    if (chosen.isValid()) {
        light.color(0) = static_cast<float>(chosen.redF());
        light.color(1) = static_cast<float>(chosen.greenF());
        light.color(2) = static_cast<float>(chosen.blueF());
        set_button_color(_scene_light_color_button,
                         static_cast<float>(chosen.redF()),
                         static_cast<float>(chosen.greenF()),
                         static_cast<float>(chosen.blueF()),
                         1.0f);
        emit scene_changed();
    }
}

// ═════════════════════════════════════════════════════════════════════
// Slots: BVH overlay
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_bvh_enabled_changed(bool checked) {
    auto *bvh = selected_bvh_data();
    if (!bvh) return;
    bvh->enabled = checked;
    bvh->mark_overlay_dirty();
    emit scene_changed();
}

void MeshControlsWidget::on_bvh_kdop_changed(int index) {
    auto *bvh = selected_bvh_data();
    if (!bvh) return;
    static constexpr int k_values[] = {3, 9, 13};
    if (index >= 0 && index < 3) {
        bvh->kdop_k = k_values[index];
        bvh->mark_dirty();
        emit scene_changed();
    }
}

void MeshControlsWidget::on_bvh_strategy_changed(int index) {
    auto *bvh = selected_bvh_data();
    if (!bvh) return;
    if (index >= 0 && index < 4) {
        bvh->strategy = static_cast<quiver::spatial::BVHBuildStrategy>(index);
        bvh->mark_dirty();
        emit scene_changed();
    }
}

void MeshControlsWidget::on_bvh_leaf_size_changed(int value) {
    auto *bvh = selected_bvh_data();
    if (!bvh) return;
    bvh->max_leaf_size = static_cast<uint16_t>(value);
    _bvh_leaf_size_label->setText(QStringLiteral("Max Leaf: %1").arg(value));
    bvh->mark_dirty();
    emit scene_changed();
}

void MeshControlsWidget::on_bvh_depth_changed(int value) {
    auto *bvh = selected_bvh_data();
    if (!bvh) return;
    bvh->display_depth = value;
    _bvh_depth_label->setText(QStringLiteral("Depth: %1").arg(value));
    bvh->mark_overlay_dirty();
    emit scene_changed();
}

void MeshControlsWidget::on_bvh_color_clicked() {
    auto *bvh = selected_bvh_data();
    if (!bvh) return;
    QColor initial = QColor::fromRgbF(
        bvh->color[0], bvh->color[1], bvh->color[2], bvh->color[3]);
    QColor chosen = QColorDialog::getColor(
        initial, this, "BVH Overlay Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        bvh->color[0] = static_cast<float>(chosen.redF());
        bvh->color[1] = static_cast<float>(chosen.greenF());
        bvh->color[2] = static_cast<float>(chosen.blueF());
        bvh->color[3] = static_cast<float>(chosen.alphaF());
        set_button_color(_bvh_color_button,
                         bvh->color[0],
                         bvh->color[1],
                         bvh->color[2],
                         bvh->color[3]);
        bvh->mark_overlay_dirty();
        emit scene_changed();
    }
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Object
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_name_edited(const QString &text) {
    if (!_selected) return;
    _selected->name = text.toStdString();
    emit scene_changed();
}

// ═════════════════════════════════════════════════════════════════════
// Slots: Transform
// ═════════════════════════════════════════════════════════════════════

void MeshControlsWidget::on_translation_changed() {
    if (!_selected) return;
    sg::Vec3f t;
    t(0) = static_cast<float>(_tx->value());
    t(1) = static_cast<float>(_ty->value());
    t(2) = static_cast<float>(_tz->value());
    _selected->set_translation(t);
    emit scene_changed();
}

void MeshControlsWidget::on_rotation_changed() {
    if (!_selected) return;
    sg::Vec3f euler;
    euler(0) = static_cast<float>(_rx->value());
    euler(1) = static_cast<float>(_ry->value());
    euler(2) = static_cast<float>(_rz->value());
    _selected->set_rotation_euler(euler);
    emit scene_changed();
}

void MeshControlsWidget::on_scale_changed() {
    if (!_selected) return;
    sg::Vec3f s;
    s(0) = static_cast<float>(_sx->value());
    s(1) = static_cast<float>(_sy->value());
    s(2) = static_cast<float>(_sz->value());
    _selected->set_scale_factors(s);
    emit scene_changed();
}

void MeshControlsWidget::on_reset_transform() {
    if (!_selected) return;
    _selected->reset_transform();
    sync_transform_from_object();
    emit scene_changed();
}

} // namespace balsa::visualization::qt
