#if !defined(BALSA_VISUALIZATION_QT_MESH_CONTROLS_WIDGET_HPP)
#define BALSA_VISUALIZATION_QT_MESH_CONTROLS_WIDGET_HPP

#include <QWidget>

// Forward declarations — avoid including heavy Vulkan/Qt headers here.
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QLineEdit;
class QPushButton;
class QGroupBox;

namespace balsa::visualization::vulkan {
class MeshScene;
struct MeshRenderState;
}// namespace balsa::visualization::vulkan

namespace balsa::scene_graph {
class Object;
class MeshData;
}// namespace balsa::scene_graph

namespace balsa::visualization::qt {

// ── MeshControlsWidget ──────────────────────────────────────────────
//
// Qt control panel for mesh-specific properties.  Shows shading, color,
// render layers (solid/wireframe/points), material, and scene lighting
// controls for the currently selected mesh Object.
//
// Render layers replace the old RenderMode enum: each layer (solid,
// wireframe, points) has an independent enable toggle and color.
// Material properties (ambient/specular/shininess) are per-mesh;
// the light source (direction, color) is scene-global.
//
// Does NOT contain scene tree / outliner controls — those belong in
// SceneGraphWidget.  The host application wires them together:
//   SceneGraphWidget::object_selected(Object*) →
//     MeshControlsWidget::set_selected_object(Object*)
//
// Emits scene_changed() whenever the user modifies any render state.

class MeshControlsWidget : public QWidget {
    Q_OBJECT

  public:
    explicit MeshControlsWidget(QWidget *parent = nullptr);
    ~MeshControlsWidget() override;

    // Set the scene for global context (e.g. accessing pipeline state).
    void set_scene(::balsa::visualization::vulkan::MeshScene *scene);

    // Set the currently selected object.  nullptr clears the panel.
    // This replaces the old internal QListWidget-based selection.
    void set_selected_object(::balsa::scene_graph::Object *obj);

  signals:
    // Emitted whenever the user changes any rendering parameter.
    void scene_changed();

  private slots:
    void on_visibility_changed(bool checked);

    // Render state
    void on_shading_changed(int index);
    void on_color_source_changed(int index);
    void on_normal_source_changed(int index);
    void on_two_sided_changed(bool checked);

    // Color
    void on_uniform_color_clicked();
    void on_colormap_changed(int index);
    void on_colormap_custom_edited();
    void on_scalar_min_changed(double value);
    void on_scalar_max_changed(double value);
    void on_scalar_range_reset();

    // Render layers
    void on_solid_enabled_changed(bool checked);
    void on_solid_color_clicked();
    void on_wireframe_enabled_changed(bool checked);
    void on_wireframe_color_clicked();
    void on_wireframe_width_changed(int value);
    void on_points_enabled_changed(bool checked);
    void on_point_color_clicked();
    void on_point_size_changed(int value);

    // Material
    void on_material_ambient_changed(int value);
    void on_material_specular_changed(int value);
    void on_material_shininess_changed(int value);

    // Scene lighting
    void on_scene_light_enabled_changed(bool checked);
    void on_scene_light_dir_changed();
    void on_scene_light_color_clicked();

    // Object
    void on_name_edited(const QString &text);

  private:
    void build_ui();
    void build_property_panel(QWidget *parent);
    void build_render_state_group(QWidget *parent);
    void build_color_group(QWidget *parent);
    void build_layers_group(QWidget *parent);
    void build_material_group(QWidget *parent);
    void build_scene_lighting_group(QWidget *parent);
    void build_object_group(QWidget *parent);

    // Populate widgets from the currently-selected object's state.
    // Blocks signals while setting values to avoid feedback loops.
    void sync_from_state();
    void sync_color_group_visibility();

    // Helper: get the MeshData feature from the selected Object, or nullptr.
    ::balsa::scene_graph::MeshData *selected_mesh_data();

    // ── State ────────────────────────────────────────────────────────
    ::balsa::visualization::vulkan::MeshScene *_scene = nullptr;
    ::balsa::scene_graph::Object *_selected = nullptr;

    // ── Object group ─────────────────────────────────────────────────
    QGroupBox *_object_group = nullptr;
    QLineEdit *_name_edit = nullptr;
    QCheckBox *_visible_check = nullptr;
    QLabel *_vertex_count_label = nullptr;
    QLabel *_triangle_count_label = nullptr;
    QLabel *_edge_count_label = nullptr;
    QLabel *_attribute_label = nullptr;

    // ── Render state group ───────────────────────────────────────────
    QGroupBox *_render_state_group = nullptr;
    QComboBox *_shading_combo = nullptr;
    QComboBox *_normal_source_combo = nullptr;
    QCheckBox *_two_sided_check = nullptr;

    // ── Color group ──────────────────────────────────────────────────
    QGroupBox *_color_group = nullptr;
    QComboBox *_color_source_combo = nullptr;
    QPushButton *_uniform_color_button = nullptr;
    QComboBox *_colormap_combo = nullptr;
    QLineEdit *_colormap_custom_edit = nullptr;
    QDoubleSpinBox *_scalar_min_spin = nullptr;
    QDoubleSpinBox *_scalar_max_spin = nullptr;
    QPushButton *_scalar_range_reset_button = nullptr;
    // Container widgets for conditional visibility
    QWidget *_uniform_color_container = nullptr;
    QWidget *_scalar_field_container = nullptr;

    // ── Render layers group ──────────────────────────────────────────
    QGroupBox *_layers_group = nullptr;
    // Solid
    QCheckBox *_solid_enabled_check = nullptr;
    QPushButton *_solid_color_button = nullptr;
    // Wireframe
    QCheckBox *_wireframe_enabled_check = nullptr;
    QPushButton *_wireframe_color_button = nullptr;
    QSlider *_wireframe_width_slider = nullptr;
    QLabel *_wireframe_width_label = nullptr;
    QWidget *_wireframe_details_container = nullptr;
    // Points
    QCheckBox *_points_enabled_check = nullptr;
    QPushButton *_point_color_button = nullptr;
    QSlider *_point_size_slider = nullptr;
    QLabel *_point_size_label = nullptr;
    QWidget *_points_details_container = nullptr;

    // ── Material group ───────────────────────────────────────────────
    QGroupBox *_material_group = nullptr;
    QSlider *_material_ambient_slider = nullptr;
    QSlider *_material_specular_slider = nullptr;
    QSlider *_material_shininess_slider = nullptr;
    QLabel *_material_ambient_label = nullptr;
    QLabel *_material_specular_label = nullptr;
    QLabel *_material_shininess_label = nullptr;

    // ── Scene lighting group ─────────────────────────────────────────
    QGroupBox *_scene_lighting_group = nullptr;
    QCheckBox *_scene_light_enabled_check = nullptr;
    QDoubleSpinBox *_scene_light_x_spin = nullptr;
    QDoubleSpinBox *_scene_light_y_spin = nullptr;
    QDoubleSpinBox *_scene_light_z_spin = nullptr;
    QPushButton *_scene_light_color_button = nullptr;
};

}// namespace balsa::visualization::qt

#endif
