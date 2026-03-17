#include "balsa/visualization/qt/mesh_controls_widget.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"
#include "balsa/visualization/vulkan/mesh_drawable.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

#include <QBoxLayout>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QString>

namespace balsa::visualization::qt {

namespace vulkan = ::balsa::visualization::vulkan;

// ── Colormap list ────────────────────────────────────────────────────
// Same list as the ImGui panel, kept in sync manually.

static const char *k_colormap_names[] = {
    // MATLAB family
    "MATLAB_jet",
    "MATLAB_parula",
    "MATLAB_hot",
    "MATLAB_cool",
    "MATLAB_spring",
    "MATLAB_summer",
    "MATLAB_autumn",
    "MATLAB_winter",
    "MATLAB_bone",
    "MATLAB_copper",
    "MATLAB_pink",
    "MATLAB_hsv",
    // transform family
    "transform_rainbow",
    "transform_seismic",
    "transform_hot_metal",
    "transform_grayscale_banded",
    "transform_space",
    "transform_supernova",
    "transform_ether",
    "transform_malachite",
    "transform_morning_glory",
    "transform_peanut_butter_and_jerry",
    "transform_purple_haze",
    "transform_rose",
    "transform_saturn",
    "transform_lava_waves",
    "transform_apricot",
    "transform_carnation",
    // IDL ColorBrewer (sequential / diverging)
    "IDL_CB-Blues",
    "IDL_CB-Greens",
    "IDL_CB-Greys",
    "IDL_CB-Oranges",
    "IDL_CB-Purples",
    "IDL_CB-Reds",
    "IDL_CB-BuGn",
    "IDL_CB-BuPu",
    "IDL_CB-GnBu",
    "IDL_CB-OrRd",
    "IDL_CB-PuBu",
    "IDL_CB-PuBuGn",
    "IDL_CB-PuOr",
    "IDL_CB-PuRd",
    "IDL_CB-RdBu",
    "IDL_CB-RdGy",
    "IDL_CB-RdPu",
    "IDL_CB-RdYiBu",
    "IDL_CB-RdYiGn",
    "IDL_CB-BrBG",
    "IDL_CB-PiYG",
    "IDL_CB-PRGn",
    "IDL_CB-Spectral",
    "IDL_CB-YIGn",
    "IDL_CB-YIGnBu",
    "IDL_CB-YIOrBr",
    // IDL ColorBrewer (qualitative)
    "IDL_CB-Accent",
    "IDL_CB-Dark2",
    "IDL_CB-Paired",
    "IDL_CB-Pastel1",
    "IDL_CB-Pastel2",
    "IDL_CB-Set1",
    "IDL_CB-Set2",
    "IDL_CB-Set3",
    // IDL classic
    "IDL_Rainbow",
    "IDL_Rainbow_2",
    "IDL_Rainbow_18",
    "IDL_Rainbow+Black",
    "IDL_Rainbow+White",
    "IDL_Blue-Red",
    "IDL_Blue-Red_2",
    "IDL_Blue-White_Linear",
    "IDL_Blue-Green-Red-Yellow",
    "IDL_Blue-Pastel-Red",
    "IDL_Blue_Waves",
    "IDL_Green-Pink",
    "IDL_Green-Red-Blue-White",
    "IDL_Green-White_Exponential",
    "IDL_Green-White_Linear",
    "IDL_Red-Purple",
    "IDL_Red_Temperature",
    "IDL_Black-White_Linear",
    "IDL_16_Level",
    "IDL_Beach",
    "IDL_Eos_A",
    "IDL_Eos_B",
    "IDL_Hardcandy",
    "IDL_Haze",
    "IDL_Hue_Sat_Lightness_1",
    "IDL_Hue_Sat_Lightness_2",
    "IDL_Hue_Sat_Value_1",
    "IDL_Hue_Sat_Value_2",
    "IDL_Mac_Style",
    "IDL_Nature",
    "IDL_Ocean",
    "IDL_Pastels",
    "IDL_Peppermint",
    "IDL_Plasma",
    "IDL_Prism",
    "IDL_Purple-Red+Stripes",
    "IDL_Standard_Gamma-II",
    "IDL_Steps",
    "IDL_Stern_Special",
    "IDL_Volcano",
    "IDL_Waves",
    // Other
    "gnuplot",
    "kbinani_altitude",
};
static const int k_colormap_count = static_cast<int>(std::size(k_colormap_names));

static int find_colormap_index(const std::string &name) {
    for (int i = 0; i < k_colormap_count; ++i) {
        if (name == k_colormap_names[i]) return i;
    }
    return -1;
}

// ── Helper: create a labeled slider ─────────────────────────────────

static QSlider *make_slider(int min, int max, int value, QWidget *parent) {
    auto *s = new QSlider(Qt::Horizontal, parent);
    s->setRange(min, max);
    s->setValue(value);
    return s;
}

// ── Helper: set QPushButton background to a color ───────────────────

static void set_button_color(QPushButton *btn, float r, float g, float b, float a) {
    QColor c = QColor::fromRgbF(r, g, b, a);
    btn->setStyleSheet(
      QStringLiteral("background-color: %1; border: 1px solid gray;").arg(c.name(QColor::HexArgb)));
}

// ── Construction / Destruction ───────────────────────────────────────

MeshControlsWidget::MeshControlsWidget(QWidget *parent)
  : QWidget(parent) {
    build_ui();
}

MeshControlsWidget::~MeshControlsWidget() = default;

// ── set_scene / refresh ──────────────────────────────────────────────

void MeshControlsWidget::set_scene(::balsa::visualization::vulkan::MeshScene *scene) {
    _scene = scene;
    refresh();
}

void MeshControlsWidget::refresh() {
    // Rebuild drawable list
    {
        QSignalBlocker block(_drawable_list);
        _drawable_list->clear();
        if (_scene) {
            for (std::size_t i = 0; i < _scene->drawable_count(); ++i) {
                auto *d = _scene->drawable(i);
                if (d) {
                    _drawable_list->addItem(QString::fromStdString(d->name));
                }
            }
        }
    }

    // Enable/disable buttons
    _add_button->setEnabled(_scene != nullptr);
    _remove_button->setEnabled(false);

    // Sync property panel
    sync_from_state();
}

// ── build_ui ─────────────────────────────────────────────────────────

void MeshControlsWidget::build_ui() {
    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(4, 4, 4, 4);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    // ── Scene panel (top) ────────────────────────────────────────────
    auto *scene_widget = new QWidget(splitter);
    build_scene_panel(scene_widget);
    splitter->addWidget(scene_widget);

    // ── Property panel (bottom, scrollable) ──────────────────────────
    auto *scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    auto *props_widget = new QWidget(scroll);
    build_property_panel(props_widget);
    scroll->setWidget(props_widget);
    splitter->addWidget(scroll);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    root_layout->addWidget(splitter);
}

void MeshControlsWidget::build_scene_panel(QWidget *parent) {
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Scene", parent));

    _drawable_list = new QListWidget(parent);
    _drawable_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(_drawable_list);

    auto *btn_layout = new QHBoxLayout;
    _add_button = new QPushButton("Add Mesh", parent);
    _remove_button = new QPushButton("Remove", parent);
    _remove_button->setEnabled(false);
    btn_layout->addWidget(_add_button);
    btn_layout->addWidget(_remove_button);
    layout->addLayout(btn_layout);

    connect(_drawable_list, &QListWidget::currentRowChanged, this, &MeshControlsWidget::on_drawable_selected);
    connect(_add_button, &QPushButton::clicked, this, &MeshControlsWidget::on_add_drawable);
    connect(_remove_button, &QPushButton::clicked, this, &MeshControlsWidget::on_remove_drawable);
}

void MeshControlsWidget::build_property_panel(QWidget *parent) {
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);

    build_object_group(parent);
    layout->addWidget(_object_group);

    build_render_state_group(parent);
    layout->addWidget(_render_state_group);

    build_color_group(parent);
    layout->addWidget(_color_group);

    build_lighting_group(parent);
    layout->addWidget(_lighting_group);

    build_wireframe_group(parent);
    layout->addWidget(_wireframe_group);

    build_points_group(parent);
    layout->addWidget(_points_group);

    layout->addStretch();
}

// ── Object group ─────────────────────────────────────────────────────

void MeshControlsWidget::build_object_group(QWidget *parent) {
    _object_group = new QGroupBox("Object", parent);
    auto *layout = new QVBoxLayout(_object_group);

    // Name
    auto *name_row = new QHBoxLayout;
    name_row->addWidget(new QLabel("Name:", _object_group));
    _name_edit = new QLineEdit(_object_group);
    name_row->addWidget(_name_edit);
    layout->addLayout(name_row);

    // Visibility
    _visible_check = new QCheckBox("Visible", _object_group);
    layout->addWidget(_visible_check);

    // Mesh info
    _vertex_count_label = new QLabel("Vertices: 0", _object_group);
    _triangle_count_label = new QLabel("Triangles: 0", _object_group);
    _edge_count_label = new QLabel("Edges: 0", _object_group);
    _attribute_label = new QLabel("Attributes: -", _object_group);
    layout->addWidget(_vertex_count_label);
    layout->addWidget(_triangle_count_label);
    layout->addWidget(_edge_count_label);
    layout->addWidget(_attribute_label);

    // Reset transform
    _reset_transform_button = new QPushButton("Reset Transform", _object_group);
    layout->addWidget(_reset_transform_button);

    connect(_name_edit, &QLineEdit::textChanged, this, &MeshControlsWidget::on_name_edited);
    connect(_visible_check, &QCheckBox::toggled, this, &MeshControlsWidget::on_visibility_changed);
    connect(_reset_transform_button, &QPushButton::clicked, this, &MeshControlsWidget::on_reset_transform);
}

// ── Render state group ───────────────────────────────────────────────

void MeshControlsWidget::build_render_state_group(QWidget *parent) {
    _render_state_group = new QGroupBox("Shading && Rendering", parent);
    auto *layout = new QVBoxLayout(_render_state_group);

    // Shading model
    auto *shading_row = new QHBoxLayout;
    shading_row->addWidget(new QLabel("Shading:", _render_state_group));
    _shading_combo = new QComboBox(_render_state_group);
    _shading_combo->addItems({ "Flat", "Gouraud", "Phong" });
    shading_row->addWidget(_shading_combo);
    layout->addLayout(shading_row);

    // Render mode
    auto *mode_row = new QHBoxLayout;
    mode_row->addWidget(new QLabel("Render Mode:", _render_state_group));
    _render_mode_combo = new QComboBox(_render_state_group);
    _render_mode_combo->addItems({ "Solid", "Wireframe", "Points", "Solid + Wireframe" });
    mode_row->addWidget(_render_mode_combo);
    layout->addLayout(mode_row);

    // Normal source
    auto *normal_row = new QHBoxLayout;
    normal_row->addWidget(new QLabel("Normals:", _render_state_group));
    _normal_source_combo = new QComboBox(_render_state_group);
    _normal_source_combo->addItems({ "From Attribute", "Computed in Shader", "None" });
    normal_row->addWidget(_normal_source_combo);
    layout->addLayout(normal_row);

    // Two-sided
    _two_sided_check = new QCheckBox("Two-Sided Lighting", _render_state_group);
    layout->addWidget(_two_sided_check);

    connect(_shading_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshControlsWidget::on_shading_changed);
    connect(_render_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshControlsWidget::on_render_mode_changed);
    connect(_normal_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshControlsWidget::on_normal_source_changed);
    connect(_two_sided_check, &QCheckBox::toggled, this, &MeshControlsWidget::on_two_sided_changed);
}

// ── Color group ──────────────────────────────────────────────────────

void MeshControlsWidget::build_color_group(QWidget *parent) {
    _color_group = new QGroupBox("Color", parent);
    auto *layout = new QVBoxLayout(_color_group);

    // Color source
    auto *src_row = new QHBoxLayout;
    src_row->addWidget(new QLabel("Source:", _color_group));
    _color_source_combo = new QComboBox(_color_group);
    _color_source_combo->addItems({ "Uniform Color", "Per-Vertex Color", "Scalar Field" });
    src_row->addWidget(_color_source_combo);
    layout->addLayout(src_row);

    // Uniform color button (hidden when not Uniform)
    _uniform_color_container = new QWidget(_color_group);
    auto *uc_layout = new QHBoxLayout(_uniform_color_container);
    uc_layout->setContentsMargins(0, 0, 0, 0);
    uc_layout->addWidget(new QLabel("Color:", _uniform_color_container));
    _uniform_color_button = new QPushButton(_uniform_color_container);
    _uniform_color_button->setFixedSize(60, 24);
    set_button_color(_uniform_color_button, 0.8f, 0.8f, 0.8f, 1.0f);
    uc_layout->addWidget(_uniform_color_button);
    uc_layout->addStretch();
    layout->addWidget(_uniform_color_container);

    // Scalar field controls (hidden when not ScalarField)
    _scalar_field_container = new QWidget(_color_group);
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

    _scalar_range_reset_button = new QPushButton("Reset Range", _scalar_field_container);
    sf_layout->addWidget(_scalar_range_reset_button);

    layout->addWidget(_scalar_field_container);

    connect(_color_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshControlsWidget::on_color_source_changed);
    connect(_uniform_color_button, &QPushButton::clicked, this, &MeshControlsWidget::on_uniform_color_clicked);
    connect(_colormap_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshControlsWidget::on_colormap_changed);
    connect(_colormap_custom_edit, &QLineEdit::editingFinished, this, &MeshControlsWidget::on_colormap_custom_edited);
    connect(_scalar_min_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshControlsWidget::on_scalar_min_changed);
    connect(_scalar_max_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshControlsWidget::on_scalar_max_changed);
    connect(_scalar_range_reset_button, &QPushButton::clicked, this, &MeshControlsWidget::on_scalar_range_reset);
}

// ── Lighting group ───────────────────────────────────────────────────

void MeshControlsWidget::build_lighting_group(QWidget *parent) {
    _lighting_group = new QGroupBox("Lighting", parent);
    _lighting_group->setCheckable(false);
    auto *layout = new QVBoxLayout(_lighting_group);

    // Light direction
    auto *dir_layout = new QHBoxLayout;
    dir_layout->addWidget(new QLabel("Dir:", _lighting_group));
    _light_x_spin = new QDoubleSpinBox(_lighting_group);
    _light_y_spin = new QDoubleSpinBox(_lighting_group);
    _light_z_spin = new QDoubleSpinBox(_lighting_group);
    for (auto *s : { _light_x_spin, _light_y_spin, _light_z_spin }) {
        s->setRange(-1.0, 1.0);
        s->setDecimals(3);
        s->setSingleStep(0.01);
        dir_layout->addWidget(s);
    }
    _light_x_spin->setValue(0.577);
    _light_y_spin->setValue(0.577);
    _light_z_spin->setValue(0.577);
    layout->addLayout(dir_layout);

    // Ambient
    auto *amb_row = new QHBoxLayout;
    _ambient_label = new QLabel("Ambient: 0.15", _lighting_group);
    amb_row->addWidget(_ambient_label);
    _ambient_slider = make_slider(0, 100, 15, _lighting_group);
    amb_row->addWidget(_ambient_slider);
    layout->addLayout(amb_row);

    // Specular
    auto *spec_row = new QHBoxLayout;
    _specular_label = new QLabel("Specular: 0.50", _lighting_group);
    spec_row->addWidget(_specular_label);
    _specular_slider = make_slider(0, 200, 50, _lighting_group);
    spec_row->addWidget(_specular_slider);
    layout->addLayout(spec_row);

    // Shininess
    auto *shin_row = new QHBoxLayout;
    _shininess_label = new QLabel("Shininess: 32", _lighting_group);
    shin_row->addWidget(_shininess_label);
    _shininess_slider = make_slider(1, 256, 32, _lighting_group);
    shin_row->addWidget(_shininess_slider);
    layout->addLayout(shin_row);

    connect(_light_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshControlsWidget::on_light_dir_changed);
    connect(_light_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshControlsWidget::on_light_dir_changed);
    connect(_light_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshControlsWidget::on_light_dir_changed);
    connect(_ambient_slider, &QSlider::valueChanged, this, &MeshControlsWidget::on_ambient_changed);
    connect(_specular_slider, &QSlider::valueChanged, this, &MeshControlsWidget::on_specular_changed);
    connect(_shininess_slider, &QSlider::valueChanged, this, &MeshControlsWidget::on_shininess_changed);
}

// ── Wireframe group ──────────────────────────────────────────────────

void MeshControlsWidget::build_wireframe_group(QWidget *parent) {
    _wireframe_group = new QGroupBox("Wireframe", parent);
    auto *layout = new QHBoxLayout(_wireframe_group);

    layout->addWidget(new QLabel("Color:", _wireframe_group));
    _wireframe_color_button = new QPushButton(_wireframe_group);
    _wireframe_color_button->setFixedSize(60, 24);
    set_button_color(_wireframe_color_button, 0.0f, 0.0f, 0.0f, 1.0f);
    layout->addWidget(_wireframe_color_button);

    layout->addWidget(new QLabel("Width:", _wireframe_group));
    _wireframe_width_slider = make_slider(5, 50, 15, _wireframe_group);// *10 scale
    layout->addWidget(_wireframe_width_slider);

    connect(_wireframe_color_button, &QPushButton::clicked, this, &MeshControlsWidget::on_wireframe_color_clicked);
    connect(_wireframe_width_slider, &QSlider::valueChanged, this, &MeshControlsWidget::on_wireframe_width_changed);
}

// ── Points group ─────────────────────────────────────────────────────

void MeshControlsWidget::build_points_group(QWidget *parent) {
    _points_group = new QGroupBox("Points", parent);
    auto *layout = new QHBoxLayout(_points_group);

    layout->addWidget(new QLabel("Size:", _points_group));
    _point_size_slider = make_slider(1, 200, 30, _points_group);// *10 scale
    layout->addWidget(_point_size_slider);

    connect(_point_size_slider, &QSlider::valueChanged, this, &MeshControlsWidget::on_point_size_changed);
}

// ── selected_drawable ────────────────────────────────────────────────

::balsa::visualization::vulkan::MeshDrawable *MeshControlsWidget::selected_drawable() {
    if (!_scene) return nullptr;
    int row = _drawable_list->currentRow();
    if (row < 0 || row >= static_cast<int>(_scene->drawable_count())) return nullptr;
    return _scene->drawable(static_cast<std::size_t>(row));
}

// ── sync_from_state ──────────────────────────────────────────────────

void MeshControlsWidget::sync_from_state() {
    auto *d = selected_drawable();
    bool have_selection = (d != nullptr);

    _object_group->setEnabled(have_selection);
    _render_state_group->setEnabled(have_selection);
    _color_group->setEnabled(have_selection);
    _lighting_group->setEnabled(have_selection);
    _wireframe_group->setEnabled(have_selection);
    _points_group->setEnabled(have_selection);
    _remove_button->setEnabled(have_selection);

    if (!d) return;

    const auto &s = d->render_state;
    const auto &buf = d->buffers();

    // Block signals to avoid feedback loops
    QSignalBlocker b1(_name_edit);
    QSignalBlocker b2(_visible_check);
    QSignalBlocker b3(_shading_combo);
    QSignalBlocker b4(_render_mode_combo);
    QSignalBlocker b5(_normal_source_combo);
    QSignalBlocker b6(_two_sided_check);
    QSignalBlocker b7(_color_source_combo);
    QSignalBlocker b8(_colormap_combo);
    QSignalBlocker b9(_colormap_custom_edit);
    QSignalBlocker b10(_scalar_min_spin);
    QSignalBlocker b11(_scalar_max_spin);
    QSignalBlocker b12(_light_x_spin);
    QSignalBlocker b13(_light_y_spin);
    QSignalBlocker b14(_light_z_spin);
    QSignalBlocker b15(_ambient_slider);
    QSignalBlocker b16(_specular_slider);
    QSignalBlocker b17(_shininess_slider);
    QSignalBlocker b18(_wireframe_width_slider);
    QSignalBlocker b19(_point_size_slider);

    // Object info
    _name_edit->setText(QString::fromStdString(d->name));
    _visible_check->setChecked(d->visible);
    _vertex_count_label->setText(QStringLiteral("Vertices: %1").arg(buf.vertex_count()));
    _triangle_count_label->setText(QStringLiteral("Triangles: %1").arg(buf.triangle_count()));
    _edge_count_label->setText(QStringLiteral("Edges: %1").arg(buf.edge_count()));

    QString attrs;
    if (buf.has_normals()) attrs += "N ";
    if (buf.has_colors()) attrs += "C ";
    if (buf.has_scalars()) attrs += "S ";
    _attribute_label->setText(QStringLiteral("Attributes: %1").arg(attrs.isEmpty() ? "-" : attrs.trimmed()));

    // Render state
    _shading_combo->setCurrentIndex(static_cast<int>(s.shading));
    _render_mode_combo->setCurrentIndex(static_cast<int>(s.render_mode));
    _normal_source_combo->setCurrentIndex(static_cast<int>(s.normal_source));
    _two_sided_check->setChecked(s.two_sided);

    // Color
    _color_source_combo->setCurrentIndex(static_cast<int>(s.color_source));
    set_button_color(_uniform_color_button,
                     s.uniform_color[0],
                     s.uniform_color[1],
                     s.uniform_color[2],
                     s.uniform_color[3]);

    int cmap_idx = find_colormap_index(s.colormap_name);
    if (cmap_idx >= 0) {
        _colormap_combo->setCurrentIndex(cmap_idx);
    }
    _colormap_custom_edit->setText(QString::fromStdString(s.colormap_name));
    _scalar_min_spin->setValue(static_cast<double>(s.scalar_min));
    _scalar_max_spin->setValue(static_cast<double>(s.scalar_max));

    sync_color_group_visibility();

    // Lighting
    _light_x_spin->setValue(static_cast<double>(s.light_dir[0]));
    _light_y_spin->setValue(static_cast<double>(s.light_dir[1]));
    _light_z_spin->setValue(static_cast<double>(s.light_dir[2]));
    _ambient_slider->setValue(static_cast<int>(s.ambient_strength * 100.0f));
    _specular_slider->setValue(static_cast<int>(s.specular_strength * 100.0f));
    _shininess_slider->setValue(static_cast<int>(s.shininess));
    _ambient_label->setText(QStringLiteral("Ambient: %1").arg(s.ambient_strength, 0, 'f', 2));
    _specular_label->setText(QStringLiteral("Specular: %1").arg(s.specular_strength, 0, 'f', 2));
    _shininess_label->setText(QStringLiteral("Shininess: %1").arg(static_cast<int>(s.shininess)));

    // Wireframe
    set_button_color(_wireframe_color_button,
                     s.wireframe_color[0],
                     s.wireframe_color[1],
                     s.wireframe_color[2],
                     s.wireframe_color[3]);
    _wireframe_width_slider->setValue(static_cast<int>(s.wireframe_width * 10.0f));

    // Points
    _point_size_slider->setValue(static_cast<int>(s.point_size * 10.0f));

    // Show/hide wireframe and points groups based on render mode
    _wireframe_group->setVisible(
      s.render_mode == vulkan::RenderMode::Wireframe || s.render_mode == vulkan::RenderMode::SolidWireframe);
    _points_group->setVisible(s.render_mode == vulkan::RenderMode::Points);
}

void MeshControlsWidget::sync_color_group_visibility() {
    auto *d = selected_drawable();
    if (!d) return;
    auto src = d->render_state.color_source;
    _uniform_color_container->setVisible(src == vulkan::ColorSource::Uniform);
    _scalar_field_container->setVisible(src == vulkan::ColorSource::ScalarField);
}

// ── Scene tree slots ─────────────────────────────────────────────────

void MeshControlsWidget::on_drawable_selected(int /*row*/) {
    sync_from_state();
}

void MeshControlsWidget::on_add_drawable() {
    if (!_scene) return;
    _scene->add_drawable("New Mesh");
    refresh();
    emit scene_changed();
}

void MeshControlsWidget::on_remove_drawable() {
    if (!_scene) return;
    int row = _drawable_list->currentRow();
    if (row < 0) return;
    _scene->remove_drawable(static_cast<std::size_t>(row));
    refresh();
    emit scene_changed();
}

void MeshControlsWidget::on_visibility_changed(bool checked) {
    auto *d = selected_drawable();
    if (!d) return;
    d->visible = checked;
    emit scene_changed();
}

// ── Render state slots ───────────────────────────────────────────────

void MeshControlsWidget::on_shading_changed(int index) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.shading = static_cast<vulkan::ShadingModel>(index);
    emit scene_changed();
}

void MeshControlsWidget::on_render_mode_changed(int index) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.render_mode = static_cast<vulkan::RenderMode>(index);
    // Update group visibility
    _wireframe_group->setVisible(
      d->render_state.render_mode == vulkan::RenderMode::Wireframe || d->render_state.render_mode == vulkan::RenderMode::SolidWireframe);
    _points_group->setVisible(d->render_state.render_mode == vulkan::RenderMode::Points);
    emit scene_changed();
}

void MeshControlsWidget::on_color_source_changed(int index) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.color_source = static_cast<vulkan::ColorSource>(index);
    sync_color_group_visibility();
    emit scene_changed();
}

void MeshControlsWidget::on_normal_source_changed(int index) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.normal_source = static_cast<vulkan::NormalSource>(index);
    emit scene_changed();
}

void MeshControlsWidget::on_two_sided_changed(bool checked) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.two_sided = checked;
    emit scene_changed();
}

// ── Color slots ──────────────────────────────────────────────────────

void MeshControlsWidget::on_uniform_color_clicked() {
    auto *d = selected_drawable();
    if (!d) return;
    auto &uc = d->render_state.uniform_color;
    QColor initial = QColor::fromRgbF(uc[0], uc[1], uc[2], uc[3]);
    QColor chosen = QColorDialog::getColor(initial, this, "Uniform Color", QColorDialog::ShowAlphaChannel);
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
    auto *d = selected_drawable();
    if (!d || index < 0 || index >= k_colormap_count) return;
    d->render_state.colormap_name = k_colormap_names[index];
    {
        QSignalBlocker block(_colormap_custom_edit);
        _colormap_custom_edit->setText(QString::fromStdString(d->render_state.colormap_name));
    }
    emit scene_changed();
}

void MeshControlsWidget::on_colormap_custom_edited() {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.colormap_name = _colormap_custom_edit->text().toStdString();
    // Try to sync the combo to match
    int idx = find_colormap_index(d->render_state.colormap_name);
    if (idx >= 0) {
        QSignalBlocker block(_colormap_combo);
        _colormap_combo->setCurrentIndex(idx);
    }
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_min_changed(double value) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.scalar_min = static_cast<float>(value);
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_max_changed(double value) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.scalar_max = static_cast<float>(value);
    emit scene_changed();
}

void MeshControlsWidget::on_scalar_range_reset() {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.scalar_min = 0.0f;
    d->render_state.scalar_max = 1.0f;
    {
        QSignalBlocker b1(_scalar_min_spin);
        QSignalBlocker b2(_scalar_max_spin);
        _scalar_min_spin->setValue(0.0);
        _scalar_max_spin->setValue(1.0);
    }
    emit scene_changed();
}

// ── Lighting slots ───────────────────────────────────────────────────

void MeshControlsWidget::on_light_dir_changed() {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.light_dir[0] = static_cast<float>(_light_x_spin->value());
    d->render_state.light_dir[1] = static_cast<float>(_light_y_spin->value());
    d->render_state.light_dir[2] = static_cast<float>(_light_z_spin->value());
    emit scene_changed();
}

void MeshControlsWidget::on_ambient_changed(int value) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.ambient_strength = static_cast<float>(value) / 100.0f;
    _ambient_label->setText(QStringLiteral("Ambient: %1").arg(d->render_state.ambient_strength, 0, 'f', 2));
    emit scene_changed();
}

void MeshControlsWidget::on_specular_changed(int value) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.specular_strength = static_cast<float>(value) / 100.0f;
    _specular_label->setText(QStringLiteral("Specular: %1").arg(d->render_state.specular_strength, 0, 'f', 2));
    emit scene_changed();
}

void MeshControlsWidget::on_shininess_changed(int value) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.shininess = static_cast<float>(value);
    _shininess_label->setText(QStringLiteral("Shininess: %1").arg(value));
    emit scene_changed();
}

// ── Wireframe / Points slots ─────────────────────────────────────────

void MeshControlsWidget::on_wireframe_color_clicked() {
    auto *d = selected_drawable();
    if (!d) return;
    auto &wc = d->render_state.wireframe_color;
    QColor initial = QColor::fromRgbF(wc[0], wc[1], wc[2], wc[3]);
    QColor chosen = QColorDialog::getColor(initial, this, "Wireframe Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        wc[0] = static_cast<float>(chosen.redF());
        wc[1] = static_cast<float>(chosen.greenF());
        wc[2] = static_cast<float>(chosen.blueF());
        wc[3] = static_cast<float>(chosen.alphaF());
        set_button_color(_wireframe_color_button, wc[0], wc[1], wc[2], wc[3]);
        emit scene_changed();
    }
}

void MeshControlsWidget::on_wireframe_width_changed(int value) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.wireframe_width = static_cast<float>(value) / 10.0f;
    emit scene_changed();
}

void MeshControlsWidget::on_point_size_changed(int value) {
    auto *d = selected_drawable();
    if (!d) return;
    d->render_state.point_size = static_cast<float>(value) / 10.0f;
    emit scene_changed();
}

// ── Object slots ─────────────────────────────────────────────────────

void MeshControlsWidget::on_name_edited(const QString &text) {
    auto *d = selected_drawable();
    if (!d) return;
    d->name = text.toStdString();
    // Update the list item text to match
    auto *item = _drawable_list->currentItem();
    if (item) {
        item->setText(text);
    }
    emit scene_changed();
}

void MeshControlsWidget::on_reset_transform() {
    auto *d = selected_drawable();
    if (!d) return;
    d->model_matrix = glm::mat4(1.0f);
    emit scene_changed();
}

}// namespace balsa::visualization::qt
