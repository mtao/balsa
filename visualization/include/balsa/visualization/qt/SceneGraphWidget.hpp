#if !defined(BALSA_VISUALIZATION_QT_SCENE_GRAPH_WIDGET_HPP)
#define BALSA_VISUALIZATION_QT_SCENE_GRAPH_WIDGET_HPP

#include <QWidget>

class QTreeView;
class QBoxLayout;
class QPushButton;
class QMenu;

namespace balsa::scene_graph {
class Object;
}

namespace balsa::visualization::qt {

class SceneGraphModel;

// ── SceneGraphWidget ────────────────────────────────────────────────
//
// A Blender-style outliner for the scene graph.
//
// Layout (simple vertical):
//   Toolbar:  Add Empty, Add Mesh, Add Camera, Delete
//   Tree:     QTreeView showing the Object hierarchy via SceneGraphModel
//             - Expand/collapse, drag-and-drop reparenting
//             - Type icons, visibility checkboxes, selectability checkboxes
//             - Right-click context menu (Add Child, Add Mesh, Rename,
//               Delete, Duplicate)
//             - Inline rename (double-click)
//
// Transform controls have been moved to MeshControlsWidget (Object tab).
//
// Signals:
//   object_selected(Object*)  -- when tree selection changes
//   scene_changed()           -- when any visibility/structure changes

class SceneGraphWidget : public QWidget {
    Q_OBJECT

  public:
    explicit SceneGraphWidget(QWidget *parent = nullptr);
    ~SceneGraphWidget() override;

    // Set the scene graph root.  nullptr clears everything.
    void set_root(::balsa::scene_graph::Object *root);

    // Force refresh (after external structural changes).
    void refresh();

    // Get the currently selected Object, or nullptr.
    ::balsa::scene_graph::Object *selected_object() const;

    // Programmatic selection.
    void select_object(::balsa::scene_graph::Object *obj);

  signals:
    // Emitted when the tree selection changes.
    void object_selected(::balsa::scene_graph::Object *obj);

    // Emitted when any scene parameter changes (structure, visibility).
    // The host should trigger a re-render.
    void scene_changed();

    // Emitted when the user requests to look through a camera Object.
    // nullptr means revert to the default camera.
    void camera_activated(::balsa::scene_graph::Object *cam_obj);

  private slots:
    void on_selection_changed();
    void on_model_structure_changed();

    // Toolbar / context menu actions
    void on_add_empty();
    void on_add_mesh();
    void on_add_camera();
    void on_delete();
    void on_duplicate();

    // Context menu
    void on_context_menu(const QPoint &pos);

  protected:
    // Deselect when clicking empty space in the tree viewport.
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void build_ui();
    void build_toolbar(QWidget *parent, QBoxLayout *layout);
    void build_tree(QWidget *parent, QBoxLayout *layout);

    // ── Widgets ─────────────────────────────────────────────────────

    SceneGraphModel *_model = nullptr;
    QTreeView *_tree = nullptr;

    // Toolbar
    QPushButton *_add_empty_btn = nullptr;
    QPushButton *_add_mesh_btn = nullptr;
    QPushButton *_add_camera_btn = nullptr;
    QPushButton *_delete_btn = nullptr;
};

}// namespace balsa::visualization::qt

#endif
