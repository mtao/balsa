#if !defined(BALSA_VISUALIZATION_QT_SCENE_GRAPH_MODEL_HPP)
#define BALSA_VISUALIZATION_QT_SCENE_GRAPH_MODEL_HPP

#include <QAbstractItemModel>
#include <QIcon>
#include <QMimeData>

namespace balsa::scene_graph {
class Object;
}

namespace balsa::visualization::qt {

// ── SceneGraphModel ─────────────────────────────────────────────────
//
// QAbstractItemModel that wraps a scene_graph::Object tree for display
// in a QTreeView.  Columns:
//
//   0  Name        (editable, with type icon as decoration)
//   1  Visibility  (checkable, eye icon)
//   2  Selectable  (checkable)
//
// Supports:
//   - Drag-and-drop reparenting via internal MIME moves
//   - Inline rename via Qt::EditRole on column 0
//   - Toggling visibility / selectability via CheckStateRole
//
// The model stores a non-owning pointer to the root Object.  Call
// set_root() to point it at a new scene graph.  Call refresh() to
// force a full model reset (e.g. after external structural changes).

class SceneGraphModel : public QAbstractItemModel {
    Q_OBJECT

  public:
    // Column indices.
    static constexpr int ColName = 0;
    static constexpr int ColVisible = 1;
    static constexpr int ColSelectable = 2;
    static constexpr int ColumnCount = 3;

    // MIME type for internal drag-and-drop.
    static constexpr const char *k_mime_type = "application/x-balsa-scene-graph-object";

    explicit SceneGraphModel(QObject *parent = nullptr);
    ~SceneGraphModel() override;

    // ── Root management ─────────────────────────────────────────────

    // Set the scene graph root.  nullptr clears the model.
    void set_root(::balsa::scene_graph::Object *root);

    // Get the current root.
    ::balsa::scene_graph::Object *root() const { return _root; }

    // Force a complete model reset (beginResetModel/endResetModel).
    void refresh();

    // ── QAbstractItemModel overrides ────────────────────────────────

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;

    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // ── Drag and drop ───────────────────────────────────────────────

    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

    // ── Helpers ─────────────────────────────────────────────────────

    // Get the Object pointer from a model index.
    static ::balsa::scene_graph::Object *object_from_index(const QModelIndex &index);

    // Get a model index for a given Object (searches the tree).
    QModelIndex index_for_object(::balsa::scene_graph::Object *obj) const;

    // ── Structural mutation (notifies the view) ─────────────────────

    // Add a child to parent_obj and notify the view.  Returns the new
    // child.  If parent_obj is nullptr, adds under root.
    ::balsa::scene_graph::Object *add_child(::balsa::scene_graph::Object *parent_obj,
                                            const std::string &name);

    // Remove an object from the tree and notify the view.  Returns
    // true if found and removed.
    bool remove_object(::balsa::scene_graph::Object *obj);

    // Duplicate an object (shallow copy of TRS, name + " copy",
    // same parent).  Returns the new object or nullptr.
    ::balsa::scene_graph::Object *duplicate_object(::balsa::scene_graph::Object *obj);

  signals:
    // Emitted when the tree structure changes (add/remove/reparent).
    void structure_changed();

  private:
    // Get the icon for an Object based on its features.
    QIcon icon_for_object(::balsa::scene_graph::Object *obj) const;

    // Get the row of an Object within its parent's children,
    // skipping permanent (hidden) children.
    int row_of(::balsa::scene_graph::Object *obj) const;

    // Count only non-permanent children of obj.
    static int visible_child_count(::balsa::scene_graph::Object *obj);

    // Get the nth non-permanent child of obj (0-indexed).
    static ::balsa::scene_graph::Object *visible_child(::balsa::scene_graph::Object *obj, int row);

    ::balsa::scene_graph::Object *_root = nullptr;
};

}// namespace balsa::visualization::qt

#endif
