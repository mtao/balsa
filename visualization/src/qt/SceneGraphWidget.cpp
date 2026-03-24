#include "balsa/visualization/qt/SceneGraphWidget.hpp"
#include "balsa/visualization/qt/SceneGraphModel.hpp"

// Qt defines 'emit' as a macro, which conflicts with TBB's
// tbb::profiling::emit().  Undefine it before pulling in headers
// that transitively include TBB (via quiver), then restore it.
#undef emit

#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/BVHData.hpp"

// Restore Qt's emit macro (expands to nothing, but needed for readability).
#define emit

#include <QBoxLayout>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeView>

namespace balsa::visualization::qt {

namespace sg = ::balsa::scene_graph;

// ── Helper: make a labeled double spin box ──────────────────────────

static QDoubleSpinBox *make_spin(const char *label, QWidget *parent, QHBoxLayout *row, double min, double max, double step) {
    row->addWidget(new QLabel(label, parent));
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setDecimals(3);
    spin->setSingleStep(step);
    spin->setKeyboardTracking(false);
    row->addWidget(spin);
    return spin;
}

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
    sync_transform_from_object();
}

void SceneGraphWidget::refresh() {
    _model->refresh();
    _tree->expandAll();
    sync_transform_from_object();
}

sg::Object *SceneGraphWidget::selected_object() const {
    auto idx = _tree->currentIndex();
    return SceneGraphModel::object_from_index(idx);
}

void SceneGraphWidget::select_object(sg::Object *obj) {
    if (!obj) {
        _tree->clearSelection();
        sync_transform_from_object();
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

    auto *splitter = new QSplitter(Qt::Vertical, this);

    // ── Top: tree + toolbar ──────────────────────────────────────────
    auto *tree_container = new QWidget(splitter);
    auto *tree_layout = new QVBoxLayout(tree_container);
    tree_layout->setContentsMargins(0, 0, 0, 0);

    build_toolbar(tree_container, tree_layout);
    build_tree(tree_container, tree_layout);
    splitter->addWidget(tree_container);

    // ── Bottom: transform panel ──────────────────────────────────────
    auto *transform_container = new QWidget(splitter);
    auto *transform_layout = new QVBoxLayout(transform_container);
    transform_layout->setContentsMargins(0, 0, 0, 0);

    build_transform_panel(transform_container, transform_layout);
    transform_layout->addStretch();
    splitter->addWidget(transform_container);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    root_layout->addWidget(splitter);
}

void SceneGraphWidget::build_toolbar(QWidget *parent, QBoxLayout *layout) {
    auto *toolbar = new QHBoxLayout;

    _add_empty_btn = new QPushButton("+ Empty", parent);
    _add_mesh_btn = new QPushButton("+ Mesh", parent);
    _delete_btn = new QPushButton("Delete", parent);
    _delete_btn->setEnabled(false);

    toolbar->addWidget(_add_empty_btn);
    toolbar->addWidget(_add_mesh_btn);
    toolbar->addWidget(_delete_btn);
    toolbar->addStretch();

    layout->addLayout(toolbar);

    connect(_add_empty_btn, &QPushButton::clicked, this, &SceneGraphWidget::on_add_empty);
    connect(_add_mesh_btn, &QPushButton::clicked, this, &SceneGraphWidget::on_add_mesh);
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

void SceneGraphWidget::build_transform_panel(QWidget *parent,
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
    _reset_btn = new QPushButton("Reset Transform", _transform_group);
    grp_layout->addWidget(_reset_btn);

    layout->addWidget(_transform_group);

    // Connect spin boxes
    for (auto *spin : { _tx, _ty, _tz }) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SceneGraphWidget::on_translation_changed);
    }
    for (auto *spin : { _rx, _ry, _rz }) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SceneGraphWidget::on_rotation_changed);
    }
    for (auto *spin : { _sx, _sy, _sz }) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SceneGraphWidget::on_scale_changed);
    }
    connect(_reset_btn, &QPushButton::clicked, this, &SceneGraphWidget::on_reset_transform);
}

// ── Sync transform from selected object ─────────────────────────────

void SceneGraphWidget::sync_transform_from_object() {
    auto *obj = selected_object();
    _transform_group->setEnabled(obj != nullptr);
    _delete_btn->setEnabled(obj != nullptr);

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

// ── Event filter: deselect on click in empty space ──────────────────

bool SceneGraphWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == _tree->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        auto idx = _tree->indexAt(me->pos());
        if (!idx.isValid()) {
            // Clicked on empty space — clear the selection.
            _tree->clearSelection();
            _tree->setCurrentIndex(QModelIndex());
            sync_transform_from_object();
            emit object_selected(nullptr);
            // Don't consume — let normal mouse handling proceed.
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── Slots: selection ────────────────────────────────────────────────

void SceneGraphWidget::on_selection_changed() {
    auto *obj = selected_object();
    sync_transform_from_object();
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

void SceneGraphWidget::on_delete() {
    auto *obj = selected_object();
    if (!obj) return;
    _model->remove_object(obj);
    sync_transform_from_object();
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

// ── Slots: transform ────────────────────────────────────────────────

void SceneGraphWidget::on_translation_changed() {
    auto *obj = selected_object();
    if (!obj) return;

    sg::Vec3f t;
    t(0) = static_cast<float>(_tx->value());
    t(1) = static_cast<float>(_ty->value());
    t(2) = static_cast<float>(_tz->value());
    obj->set_translation(t);

    emit scene_changed();
}

void SceneGraphWidget::on_rotation_changed() {
    auto *obj = selected_object();
    if (!obj) return;

    sg::Vec3f euler;
    euler(0) = static_cast<float>(_rx->value());
    euler(1) = static_cast<float>(_ry->value());
    euler(2) = static_cast<float>(_rz->value());
    obj->set_rotation_euler(euler);

    emit scene_changed();
}

void SceneGraphWidget::on_scale_changed() {
    auto *obj = selected_object();
    if (!obj) return;

    sg::Vec3f s;
    s(0) = static_cast<float>(_sx->value());
    s(1) = static_cast<float>(_sy->value());
    s(2) = static_cast<float>(_sz->value());
    obj->set_scale_factors(s);

    emit scene_changed();
}

void SceneGraphWidget::on_reset_transform() {
    auto *obj = selected_object();
    if (!obj) return;
    obj->reset_transform();
    sync_transform_from_object();
    emit scene_changed();
}

// ── Context menu ────────────────────────────────────────────────────

void SceneGraphWidget::on_context_menu(const QPoint &pos) {
    auto idx = _tree->indexAt(pos);
    auto *obj = SceneGraphModel::object_from_index(idx);

    QMenu menu(this);

    auto *add_child_action = menu.addAction("Add Child (Empty)");
    auto *add_mesh_action = menu.addAction("Add Child (Mesh)");
    menu.addSeparator();

    QAction *rename_action = nullptr;
    QAction *delete_action = nullptr;
    QAction *duplicate_action = nullptr;
    QAction *add_bvh_action = nullptr;
    QAction *remove_bvh_action = nullptr;

    if (obj) {
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
