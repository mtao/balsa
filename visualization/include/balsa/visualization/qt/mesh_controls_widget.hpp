#if !defined(BALSA_VISUALIZATION_QT_MESH_CONTROLS_WIDGET_HPP)
#define BALSA_VISUALIZATION_QT_MESH_CONTROLS_WIDGET_HPP

#include <QWidget>

// Forward declarations — avoid including heavy Vulkan/Qt headers here.
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QListWidget;
class QLineEdit;
class QPushButton;
class QGroupBox;
class QStackedWidget;

namespace balsa::visualization::vulkan {
class MeshScene;
class MeshDrawable;
struct MeshRenderState;
}// namespace balsa::visualization::vulkan

namespace balsa::visualization::qt {

// ── MeshControlsWidget ──────────────────────────────────────────────
//
// Qt control panel for the Vulkan mesh viewer.  Mirrors the ImGui
// mesh_controls_panel functionality using native Qt widgets.
//
// Operates on a MeshScene pointer — set via set_scene().  When the
// scene changes (drawables added/removed, selection changes) call
// refresh() to rebuild the UI state.
//
// Emits scene_changed() whenever the user modifies any render state,
// visibility flag, transform, etc.  The host window should connect
// this signal to trigger a re-render.
//
// Designed to be embedded as a dock widget or sidebar panel in a
// QMainWindow.

class MeshControlsWidget : public QWidget {
    Q_OBJECT

  public:
    explicit MeshControlsWidget(QWidget *parent = nullptr);
    ~MeshControlsWidget() override;

    // Set the scene to control.  nullptr clears the panel.
    void set_scene(::balsa::visualization::vulkan::MeshScene *scene);

    // Refresh the drawable list and property display from the scene.
    // Call after adding/removing drawables externally.
    void refresh();

  signals:
    // Emitted whenever the user changes any rendering parameter.
    void scene_changed();

  private slots:
    // Scene tree
    void on_drawable_selected(int row);
    void on_add_drawable();
    void on_remove_drawable();
    void on_visibility_changed(bool checked);

    // Render state
    void on_shading_changed(int index);
    void on_render_mode_changed(int index);
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

    // Lighting
    void on_light_dir_changed();
    void on_ambient_changed(int value);
    void on_specular_changed(int value);
    void on_shininess_changed(int value);

    // Wireframe / Points
    void on_wireframe_color_clicked();
    void on_wireframe_width_changed(int value);
    void on_point_size_changed(int value);

    // Object
    void on_name_edited(const QString &text);
    void on_reset_transform();

  private:
    void build_ui();
    void build_scene_panel(QWidget *parent);
    void build_property_panel(QWidget *parent);
    void build_render_state_group(QWidget *parent);
    void build_color_group(QWidget *parent);
    void build_lighting_group(QWidget *parent);
    void build_wireframe_group(QWidget *parent);
    void build_points_group(QWidget *parent);
    void build_object_group(QWidget *parent);

    // Populate widgets from the currently-selected drawable's state.
    // Blocks signals while setting values to avoid feedback loops.
    void sync_from_state();
    void sync_color_group_visibility();

    // Helper: get the currently-selected drawable, or nullptr.
    ::balsa::visualization::vulkan::MeshDrawable *selected_drawable();

    // ── State ────────────────────────────────────────────────────────
    ::balsa::visualization::vulkan::MeshScene *_scene = nullptr;

    // ── Scene panel widgets ──────────────────────────────────────────
    QListWidget *_drawable_list = nullptr;
    QPushButton *_add_button = nullptr;
    QPushButton *_remove_button = nullptr;

    // ── Object group ─────────────────────────────────────────────────
    QGroupBox *_object_group = nullptr;
    QLineEdit *_name_edit = nullptr;
    QCheckBox *_visible_check = nullptr;
    QLabel *_vertex_count_label = nullptr;
    QLabel *_triangle_count_label = nullptr;
    QLabel *_edge_count_label = nullptr;
    QLabel *_attribute_label = nullptr;
    QPushButton *_reset_transform_button = nullptr;

    // ── Render state group ───────────────────────────────────────────
    QGroupBox *_render_state_group = nullptr;
    QComboBox *_shading_combo = nullptr;
    QComboBox *_render_mode_combo = nullptr;
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

    // ── Lighting group ───────────────────────────────────────────────
    QGroupBox *_lighting_group = nullptr;
    QDoubleSpinBox *_light_x_spin = nullptr;
    QDoubleSpinBox *_light_y_spin = nullptr;
    QDoubleSpinBox *_light_z_spin = nullptr;
    QSlider *_ambient_slider = nullptr;
    QSlider *_specular_slider = nullptr;
    QSlider *_shininess_slider = nullptr;
    QLabel *_ambient_label = nullptr;
    QLabel *_specular_label = nullptr;
    QLabel *_shininess_label = nullptr;

    // ── Wireframe group ──────────────────────────────────────────────
    QGroupBox *_wireframe_group = nullptr;
    QPushButton *_wireframe_color_button = nullptr;
    QSlider *_wireframe_width_slider = nullptr;

    // ── Points group ─────────────────────────────────────────────────
    QGroupBox *_points_group = nullptr;
    QSlider *_point_size_slider = nullptr;
};

}// namespace balsa::visualization::qt

#endif
