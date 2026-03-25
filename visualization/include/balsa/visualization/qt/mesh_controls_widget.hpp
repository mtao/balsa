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
class QBoxLayout;

namespace balsa::visualization::vulkan {
class MeshScene;
struct MeshRenderState;
}// namespace balsa::visualization::vulkan

namespace balsa::scene_graph {
class Object;
class MeshData;
class BVHData;
}// namespace balsa::scene_graph

namespace balsa::visualization::qt {

class VerticalTabWidget;

// ── MeshControlsWidget ──────────────────────────────────────────────
//
// Qt property panel for the currently selected scene graph object.
// Uses a VerticalTabWidget with 5 tabs:
//
//   1. Object   — Name, visibility, mesh info, Transform (T/R/S)
//   2. Layers   — Solid/wireframe/points enables + colors + sizes,
//                  Shading & Rendering, Color source
//   3. Material — Material response (ambient, diffuse, specular,
//                  shininess).  Placeholder for future named materials.
//   4. BVH      — BVH overlay (enable, KDOP type, strategy, leaf size,
//                  depth, color)
//   5. Lighting — Scene-global light (direction, color, enable)
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
    void on_cull_mode_changed(int index);

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
    void on_material_diffuse_changed(int value);
    void on_material_specular_changed(int value);
    void on_material_shininess_changed(int value);

    // Scene lighting
    void on_scene_light_enabled_changed(bool checked);
    void on_scene_light_dir_changed();
    void on_scene_light_color_clicked();

    // BVH overlay
    void on_bvh_enabled_changed(bool checked);
    void on_bvh_kdop_changed(int index);
    void on_bvh_strategy_changed(int index);
    void on_bvh_leaf_size_changed(int value);
    void on_bvh_depth_changed(int value);
    void on_bvh_color_clicked();

    // Object
    void on_name_edited(const QString &text);

    // Transform
    void on_translation_changed();
    void on_rotation_changed();
    void on_scale_changed();
    void on_reset_transform();

  private:
    void build_ui();

    // Per-tab page builders.  Each returns a new QWidget to be added
    // as a tab page in the VerticalTabWidget.
    QWidget *build_object_page();
    QWidget *build_layers_page();
    QWidget *build_material_page();
    QWidget *build_bvh_page();
    QWidget *build_lighting_page();

    // Sub-group builders (called from page builders).
    void build_object_info(QWidget *parent, QBoxLayout *layout);
    void build_transform_section(QWidget *parent, QBoxLayout *layout);
    void build_render_state_group(QWidget *parent, QBoxLayout *layout);
    void build_color_group(QWidget *parent, QBoxLayout *layout);
    void build_layers_group(QWidget *parent, QBoxLayout *layout);
    void build_material_group(QWidget *parent, QBoxLayout *layout);
    void build_scene_lighting_group(QWidget *parent, QBoxLayout *layout);
    void build_bvh_group(QWidget *parent, QBoxLayout *layout);

    // Populate widgets from the currently-selected object's state.
    // Blocks signals while setting values to avoid feedback loops.
    void sync_from_state();
    void sync_color_group_visibility();
    void sync_transform_from_object();

    // Helper: get the MeshData feature from the selected Object, or nullptr.
    ::balsa::scene_graph::MeshData *selected_mesh_data();

    // Helper: get the BVHData feature from the selected Object, or nullptr.
    ::balsa::scene_graph::BVHData *selected_bvh_data();

    // ── State ────────────────────────────────────────────────────────
    ::balsa::visualization::vulkan::MeshScene *_scene = nullptr;
    ::balsa::scene_graph::Object *_selected = nullptr;

    // ── Tab widget ───────────────────────────────────────────────────
    VerticalTabWidget *_tabs = nullptr;

    // Tab indices (for show/hide/enable)
    int _tab_object = -1;
    int _tab_layers = -1;
    int _tab_material = -1;
    int _tab_bvh = -1;
    int _tab_lighting = -1;

    // ── Object info widgets ──────────────────────────────────────────
    QLineEdit *_name_edit = nullptr;
    QCheckBox *_visible_check = nullptr;
    QLabel *_vertex_count_label = nullptr;
    QLabel *_triangle_count_label = nullptr;
    QLabel *_edge_count_label = nullptr;
    QLabel *_attribute_label = nullptr;

    // ── Transform widgets ────────────────────────────────────────────
    QGroupBox *_transform_group = nullptr;
    QDoubleSpinBox *_tx = nullptr;
    QDoubleSpinBox *_ty = nullptr;
    QDoubleSpinBox *_tz = nullptr;
    QDoubleSpinBox *_rx = nullptr;
    QDoubleSpinBox *_ry = nullptr;
    QDoubleSpinBox *_rz = nullptr;
    QDoubleSpinBox *_sx = nullptr;
    QDoubleSpinBox *_sy = nullptr;
    QDoubleSpinBox *_sz = nullptr;
    QPushButton *_reset_transform_btn = nullptr;

    // ── Render state widgets ─────────────────────────────────────────
    QComboBox *_shading_combo = nullptr;
    QComboBox *_normal_source_combo = nullptr;
    QCheckBox *_two_sided_check = nullptr;
    QComboBox *_cull_mode_combo = nullptr;
    QWidget *_shading_details_container = nullptr;// shown only when lit

    // ── Color widgets ────────────────────────────────────────────────
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

    // ── Render layers widgets ────────────────────────────────────────
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

    // ── Material widgets ─────────────────────────────────────────────
    QSlider *_material_ambient_slider = nullptr;
    QSlider *_material_diffuse_slider = nullptr;
    QSlider *_material_specular_slider = nullptr;
    QSlider *_material_shininess_slider = nullptr;
    QLabel *_material_ambient_label = nullptr;
    QLabel *_material_diffuse_label = nullptr;
    QLabel *_material_specular_label = nullptr;
    QLabel *_material_shininess_label = nullptr;

    // ── Scene lighting widgets ───────────────────────────────────────
    QCheckBox *_scene_light_enabled_check = nullptr;
    QDoubleSpinBox *_scene_light_x_spin = nullptr;
    QDoubleSpinBox *_scene_light_y_spin = nullptr;
    QDoubleSpinBox *_scene_light_z_spin = nullptr;
    QPushButton *_scene_light_color_button = nullptr;

    // ── BVH overlay widgets ──────────────────────────────────────────
    QCheckBox *_bvh_enabled_check = nullptr;
    QComboBox *_bvh_kdop_combo = nullptr;
    QComboBox *_bvh_strategy_combo = nullptr;
    QSlider *_bvh_leaf_size_slider = nullptr;
    QLabel *_bvh_leaf_size_label = nullptr;
    QSlider *_bvh_depth_slider = nullptr;
    QLabel *_bvh_depth_label = nullptr;
    QLabel *_bvh_height_label = nullptr;
    QPushButton *_bvh_color_button = nullptr;
};

}// namespace balsa::visualization::qt

#endif
