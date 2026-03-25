#include "balsa/visualization/qt/SceneGraphWidget.hpp"
#include "balsa/visualization/qt/SceneGraphModel.hpp"

// Qt defines 'emit' as a macro, which conflicts with TBB's
// tbb::profiling::emit().  Undefine it before pulling in headers
// that transitively include TBB (via quiver), then restore it.
#undef emit

#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/BVHData.hpp"

// Restore Qt's emit macro (expands to nothing, but needed for readability).
#define emit

#include <QBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QTreeView>

namespace balsa::visualization::qt {

namespace sg = ::balsa::scene_graph;

// ── Construction / Destruction ──────────────────────────────────────

SceneGraphWidget::SceneGraphWidget(QWidget *parent)
  : QWidget(parent) {
    build_ui();
}

SceneGraphWidget::~SceneGraphWidget() = default;

// ── Public API ──────────────────────────────────────────────────────

void SceneGraphWidget::set_root(sg::Object *root) {
    _model->set_root(root);
    _tree->expandAll();
}

void SceneGraphWidget::refresh() {
    _model->refresh();
    _tree->expandAll();
}

sg::Object *SceneGraphWidget::selected_object() const {
    auto idx = _tree->currentIndex();
    return SceneGraphModel::object_from_index(idx);
}

void SceneGraphWidget::select_object(sg::Object *obj) {
    if (!obj) {
        _tree->clearSelection();
        return;
    }
    auto idx = _model->index_for_object(obj);
    if (idx.isValid()) {
        _tree->setCurrentIndex(idx);
    }
}

// ── Build UI ────────────────────────────────────────────────────────

void SceneGraphWidget::build_ui() {
    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(4, 4, 4, 4);

    build_toolbar(this, root_layout);
    build_tree(this, root_layout);
}

void SceneGraphWidget::build_toolbar(QWidget *parent, QBoxLayout *layout) {
    auto *toolbar = new QHBoxLayout;

    _add_empty_btn = new QPushButton("+ Empty", parent);
    _add_mesh_btn = new QPushButton("+ Mesh", parent);
    _add_camera_btn = new QPushButton("+ Camera", parent);
    _delete_btn = new QPushButton("Delete", parent);
    _delete_btn->setEnabled(false);

    toolbar->addWidget(_add_empty_btn);
    toolbar->addWidget(_add_mesh_btn);
    toolbar->addWidget(_add_camera_btn);
    toolbar->addWidget(_delete_btn);
    toolbar->addStretch();

    layout->addLayout(toolbar);

    connect(_add_empty_btn, &QPushButton::clicked, this, &SceneGraphWidget::on_add_empty);
    connect(_add_mesh_btn, &QPushButton::clicked, this, &SceneGraphWidget::on_add_mesh);
    connect(_add_camera_btn, &QPushButton::clicked, this, &SceneGraphWidget::on_add_camera);
    connect(_delete_btn, &QPushButton::clicked, this, &SceneGraphWidget::on_delete);
}

void SceneGraphWidget::build_tree(QWidget *parent, QBoxLayout *layout) {
    _model = new SceneGraphModel(this);

    _tree = new QTreeView(parent);
    _tree->setModel(_model);
    _tree->setSelectionMode(QAbstractItemView::SingleSelection);
    _tree->setDragEnabled(true);
    _tree->setAcceptDrops(true);
    _tree->setDropIndicatorShown(true);
    _tree->setDragDropMode(QAbstractItemView::InternalMove);
    _tree->setContextMenuPolicy(Qt::CustomContextMenu);
    _tree->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::EditKeyPressed);

    // Column sizing.
    _tree->header()->setStretchLastSection(false);
    _tree->header()->setSectionResizeMode(SceneGraphModel::ColName,
                                          QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(SceneGraphModel::ColVisible,
                                          QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(SceneGraphModel::ColSelectable,
                                          QHeaderView::ResizeToContents);

    layout->addWidget(_tree);

    // Install event filter on the viewport to detect clicks on empty space.
    _tree->viewport()->installEventFilter(this);

    connect(_tree->selectionModel(), &QItemSelectionModel::currentChanged, this, &SceneGraphWidget::on_selection_changed);
    connect(_tree, &QTreeView::customContextMenuRequested, this, &SceneGraphWidget::on_context_menu);
    connect(_model, &SceneGraphModel::structure_changed, this, &SceneGraphWidget::on_model_structure_changed);
}

// ── Event filter: deselect on click in empty space ──────────────────

bool SceneGraphWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == _tree->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        auto idx = _tree->indexAt(me->pos());
        if (!idx.isValid()) {
            // Clicked on empty space — clear the selection.
            _tree->clearSelection();
            _tree->setCurrentIndex(QModelIndex());
            emit object_selected(nullptr);
            // Don't consume — let normal mouse handling proceed.
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── Slots: selection ────────────────────────────────────────────────

void SceneGraphWidget::on_selection_changed() {
    auto *obj = selected_object();
    _delete_btn->setEnabled(obj != nullptr);
    emit object_selected(obj);
}

void SceneGraphWidget::on_model_structure_changed() {
    emit scene_changed();
}

// ── Slots: toolbar / context menu ───────────────────────────────────

void SceneGraphWidget::on_add_empty() {
    auto *parent_obj = selected_object();
    auto *child = _model->add_child(parent_obj, "Empty");
    if (child) {
        select_object(child);
    }
    emit scene_changed();
}

void SceneGraphWidget::on_add_mesh() {
    auto *parent_obj = selected_object();
    auto *child = _model->add_child(parent_obj, "Mesh");
    if (child) {
        child->emplace_feature<sg::MeshData>();
        select_object(child);
    }
    emit scene_changed();
}

void SceneGraphWidget::on_add_camera() {
    auto *parent_obj = selected_object();
    auto *child = _model->add_child(parent_obj, "Camera");
    if (child) {
        child->emplace_feature<sg::Camera>();
        select_object(child);
    }
    emit scene_changed();
}

void SceneGraphWidget::on_delete() {
    auto *obj = selected_object();
    if (!obj) return;
    _model->remove_object(obj);
    emit scene_changed();
}

void SceneGraphWidget::on_duplicate() {
    auto *obj = selected_object();
    if (!obj) return;
    auto *dup = _model->duplicate_object(obj);
    if (dup) {
        select_object(dup);
    }
    emit scene_changed();
}

// ── Context menu ────────────────────────────────────────────────────

void SceneGraphWidget::on_context_menu(const QPoint &pos) {
    auto idx = _tree->indexAt(pos);
    auto *obj = SceneGraphModel::object_from_index(idx);

    QMenu menu(this);

    auto *add_child_action = menu.addAction("Add Child (Empty)");
    auto *add_mesh_action = menu.addAction("Add Child (Mesh)");
    auto *add_camera_action = menu.addAction("Add Child (Camera)");
    menu.addSeparator();

    QAction *rename_action = nullptr;
    QAction *delete_action = nullptr;
    QAction *duplicate_action = nullptr;
    QAction *add_bvh_action = nullptr;
    QAction *remove_bvh_action = nullptr;
    QAction *look_through_action = nullptr;
    QAction *revert_camera_action = nullptr;

    if (obj) {
        // Camera-specific: Look Through / Revert
        if (obj->find_feature<sg::Camera>()) {
            look_through_action = menu.addAction("Look Through Camera");
            revert_camera_action = menu.addAction("Revert to Default Camera");
            menu.addSeparator();
        }

        rename_action = menu.addAction("Rename");
        duplicate_action = menu.addAction("Duplicate");
        menu.addSeparator();
        delete_action = menu.addAction("Delete");

        // BVH overlay (only for mesh objects with triangles)
        auto *mesh_data = obj->find_feature<sg::MeshData>();
        if (mesh_data && mesh_data->has_triangle_indices()) {
            menu.addSeparator();
            auto *bvh = obj->find_feature<sg::BVHData>();
            if (!bvh) {
                add_bvh_action = menu.addAction("Add BVH Overlay");
            } else {
                remove_bvh_action = menu.addAction("Remove BVH Overlay");
            }
        }
    }

    auto *chosen = menu.exec(_tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == add_child_action) {
        on_add_empty();
    } else if (chosen == add_mesh_action) {
        on_add_mesh();
    } else if (chosen == add_camera_action) {
        on_add_camera();
    } else if (chosen == look_through_action && obj) {
        emit camera_activated(obj);
    } else if (chosen == revert_camera_action && obj) {
        emit camera_activated(nullptr);
    } else if (chosen == rename_action && obj) {
        // Trigger inline editing on the name column.
        auto name_idx = _model->index_for_object(obj);
        if (name_idx.isValid()) {
            _tree->edit(name_idx);
        }
    } else if (chosen == duplicate_action && obj) {
        on_duplicate();
    } else if (chosen == delete_action && obj) {
        on_delete();
    } else if (chosen == add_bvh_action && obj) {
        auto &bvh = obj->emplace_feature<sg::BVHData>();
        bvh.mark_dirty();
        emit scene_changed();
    } else if (chosen == remove_bvh_action && obj) {
        auto *bvh = obj->find_feature<sg::BVHData>();
        if (bvh) {
            bvh->mark_for_removal();
        }
        emit scene_changed();
    }
}

}// namespace balsa::visualization::qt
