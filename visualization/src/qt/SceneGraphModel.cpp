#include "balsa/visualization/qt/SceneGraphModel.hpp"

// Qt defines 'emit' as a macro, which conflicts with TBB's
// tbb::profiling::emit().  Undefine it before pulling in headers
// that transitively include TBB (via quiver), then restore it.
#undef emit

#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/MeshData.hpp"

// Restore Qt's emit macro.
#define emit

#include <QDataStream>
#include <QIODevice>

#include <stack>

namespace balsa::visualization::qt {

namespace sg = ::balsa::scene_graph;

// ── Construction / Destruction ──────────────────────────────────────

SceneGraphModel::SceneGraphModel(QObject *parent)
  : QAbstractItemModel(parent) {}

SceneGraphModel::~SceneGraphModel() = default;

// ── Root management ─────────────────────────────────────────────────

void SceneGraphModel::set_root(sg::Object *root) {
    beginResetModel();
    _root = root;
    endResetModel();
}

void SceneGraphModel::refresh() {
    beginResetModel();
    endResetModel();
}

// ── QAbstractItemModel: index / parent / rowCount / columnCount ─────

QModelIndex SceneGraphModel::index(int row, int column, const QModelIndex &parent) const {
    if (!_root) return {};
    if (column < 0 || column >= ColumnCount) return {};

    sg::Object *parent_obj = parent.isValid()
                               ? static_cast<sg::Object *>(parent.internalPointer())
                               : _root;

    if (!parent_obj) return {};
    auto &children = parent_obj->children();
    if (row < 0 || row >= static_cast<int>(children.size())) return {};

    return createIndex(row, column, children[static_cast<std::size_t>(row)].get());
}

QModelIndex SceneGraphModel::parent(const QModelIndex &index) const {
    if (!index.isValid() || !_root) return {};

    auto *obj = static_cast<sg::Object *>(index.internalPointer());
    if (!obj) return {};

    auto *par = obj->parent();
    if (!par || par == _root) return {};

    // Find the row of par within its own parent.
    int r = row_of(par);
    if (r < 0) return {};
    return createIndex(r, 0, par);
}

int SceneGraphModel::rowCount(const QModelIndex &parent) const {
    if (!_root) return 0;

    sg::Object *obj = parent.isValid()
                        ? static_cast<sg::Object *>(parent.internalPointer())
                        : _root;
    if (!obj) return 0;
    return static_cast<int>(obj->children_count());
}

int SceneGraphModel::columnCount(const QModelIndex & /*parent*/) const {
    return ColumnCount;
}

// ── data ────────────────────────────────────────────────────────────

QVariant SceneGraphModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return {};
    auto *obj = static_cast<sg::Object *>(index.internalPointer());
    if (!obj) return {};

    switch (index.column()) {
    case ColName:
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return QString::fromStdString(obj->name);
        }
        if (role == Qt::DecorationRole) {
            return const_cast<SceneGraphModel *>(this)->icon_for_object(obj);
        }
        break;

    case ColVisible:
        if (role == Qt::CheckStateRole) {
            return obj->visible ? Qt::Checked : Qt::Unchecked;
        }
        if (role == Qt::ToolTipRole) {
            return QStringLiteral("Visibility");
        }
        break;

    case ColSelectable:
        if (role == Qt::CheckStateRole) {
            return obj->selectable ? Qt::Checked : Qt::Unchecked;
        }
        if (role == Qt::ToolTipRole) {
            return QStringLiteral("Selectability");
        }
        break;
    }

    return {};
}

// ── setData ─────────────────────────────────────────────────────────

bool SceneGraphModel::setData(const QModelIndex &index,
                              const QVariant &value,
                              int role) {
    if (!index.isValid()) return false;
    auto *obj = static_cast<sg::Object *>(index.internalPointer());
    if (!obj) return false;

    switch (index.column()) {
    case ColName:
        if (role == Qt::EditRole) {
            obj->name = value.toString().toStdString();
            emit dataChanged(index, index, { role });
            return true;
        }
        break;

    case ColVisible:
        if (role == Qt::CheckStateRole) {
            obj->visible = (value.toInt() == Qt::Checked);
            emit dataChanged(index, index, { role });
            emit structure_changed();
            return true;
        }
        break;

    case ColSelectable:
        if (role == Qt::CheckStateRole) {
            obj->selectable = (value.toInt() == Qt::Checked);
            emit dataChanged(index, index, { role });
            return true;
        }
        break;
    }

    return false;
}

// ── flags ───────────────────────────────────────────────────────────

Qt::ItemFlags SceneGraphModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) {
        // Dropping on the root area is allowed.
        return Qt::ItemIsDropEnabled;
    }

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable
                      | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    switch (index.column()) {
    case ColName:
        f |= Qt::ItemIsEditable;
        break;
    case ColVisible:
    case ColSelectable:
        f |= Qt::ItemIsUserCheckable;
        break;
    }

    return f;
}

// ── headerData ──────────────────────────────────────────────────────

QVariant SceneGraphModel::headerData(int section,
                                     Qt::Orientation orientation,
                                     int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColName:
        return QStringLiteral("Name");
    case ColVisible:
        return QStringLiteral("Vis");
    case ColSelectable:
        return QStringLiteral("Sel");
    }
    return {};
}

// ── Drag and drop ───────────────────────────────────────────────────

Qt::DropActions SceneGraphModel::supportedDropActions() const {
    return Qt::MoveAction;
}

QStringList SceneGraphModel::mimeTypes() const {
    return { QString::fromLatin1(k_mime_type) };
}

QMimeData *SceneGraphModel::mimeData(const QModelIndexList &indexes) const {
    if (indexes.isEmpty()) return nullptr;

    auto *mime = new QMimeData;
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);

    // Encode pointer addresses for the dragged objects.
    // Only consider column-0 indexes to avoid duplicates.
    for (auto &idx : indexes) {
        if (idx.isValid() && idx.column() == 0) {
            stream << reinterpret_cast<quintptr>(idx.internalPointer());
        }
    }

    mime->setData(QString::fromLatin1(k_mime_type), encoded);
    return mime;
}

bool SceneGraphModel::dropMimeData(const QMimeData *data,
                                   Qt::DropAction action,
                                   int row,
                                   int /*column*/,
                                   const QModelIndex &parent) {
    if (action != Qt::MoveAction) return false;
    if (!data || !data->hasFormat(QString::fromLatin1(k_mime_type))) return false;

    QByteArray encoded = data->data(QString::fromLatin1(k_mime_type));
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    sg::Object *new_parent = parent.isValid()
                               ? static_cast<sg::Object *>(parent.internalPointer())
                               : _root;
    if (!new_parent) return false;

    while (!stream.atEnd()) {
        quintptr ptr = 0;
        stream >> ptr;
        auto *obj = reinterpret_cast<sg::Object *>(ptr);
        if (!obj || obj == _root || obj == new_parent) continue;

        // Prevent reparenting an ancestor under itself.
        bool is_ancestor = false;
        for (auto *p = new_parent; p; p = p->parent()) {
            if (p == obj) {
                is_ancestor = true;
                break;
            }
        }
        if (is_ancestor) continue;

        // Find old position for proper notification.
        auto *old_parent = obj->parent();
        int old_row = row_of(obj);
        if (!old_parent || old_row < 0) continue;

        QModelIndex old_parent_idx = index_for_object(old_parent);
        QModelIndex new_parent_idx = (new_parent == _root)
                                       ? QModelIndex{}
                                       : index_for_object(new_parent);

        int dest_row = (row >= 0) ? row : static_cast<int>(new_parent->children_count());

        // Adjust dest_row if moving within the same parent.
        if (old_parent == new_parent && old_row < dest_row) {
            --dest_row;
        }

        beginRemoveRows(old_parent_idx, old_row, old_row);
        auto owned = obj->detach();
        endRemoveRows();

        if (!owned) continue;

        int insert_row = (row >= 0 && row <= static_cast<int>(new_parent->children_count()))
                           ? row
                           : static_cast<int>(new_parent->children_count());

        beginInsertRows(new_parent_idx, insert_row, insert_row);
        new_parent->add_child(std::move(owned));
        endInsertRows();
    }

    emit structure_changed();
    return true;
}

// ── Helpers ─────────────────────────────────────────────────────────

sg::Object *SceneGraphModel::object_from_index(const QModelIndex &index) {
    if (!index.isValid()) return nullptr;
    return static_cast<sg::Object *>(index.internalPointer());
}

QModelIndex SceneGraphModel::index_for_object(sg::Object *obj) const {
    if (!obj || obj == _root) return {};

    int r = row_of(obj);
    if (r < 0) return {};
    return createIndex(r, 0, obj);
}

int SceneGraphModel::row_of(sg::Object *obj) const {
    if (!obj) return -1;
    auto *par = obj->parent();
    if (!par) return -1;

    auto &siblings = par->children();
    for (std::size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == obj) return static_cast<int>(i);
    }
    return -1;
}

QIcon SceneGraphModel::icon_for_object(sg::Object *obj) const {
    if (!obj) return {};

    // Use built-in standard pixmap icons as simple type indicators.
    // Camera feature → "camera" style icon
    // MeshData feature → "mesh" style icon
    // Neither → generic "empty" style icon
    //
    // For now we return empty QIcons.  A production build would load
    // themed SVG/PNG icons from Qt resources.  The model is structured
    // to support that -- just replace the return values here.
    if (obj->find_feature<sg::Camera>()) {
        // Camera icon placeholder
        return QIcon::fromTheme(QStringLiteral("camera-video"));
    }
    if (obj->find_feature<sg::MeshData>()) {
        // Mesh icon placeholder
        return QIcon::fromTheme(QStringLiteral("application-x-blender"));
    }
    // Empty/transform node
    return QIcon::fromTheme(QStringLiteral("folder"));
}

// ── Structural mutation ─────────────────────────────────────────────

sg::Object *SceneGraphModel::add_child(sg::Object *parent_obj,
                                       const std::string &name) {
    if (!_root) return nullptr;
    if (!parent_obj) parent_obj = _root;

    QModelIndex parent_idx = (parent_obj == _root)
                               ? QModelIndex{}
                               : index_for_object(parent_obj);

    int row = static_cast<int>(parent_obj->children_count());
    beginInsertRows(parent_idx, row, row);
    auto &child = parent_obj->add_child(name);
    endInsertRows();

    emit structure_changed();
    return &child;
}

bool SceneGraphModel::remove_object(sg::Object *obj) {
    if (!obj || !_root || obj == _root) return false;

    auto *par = obj->parent();
    if (!par) return false;

    int r = row_of(obj);
    if (r < 0) return false;

    QModelIndex parent_idx = (par == _root)
                               ? QModelIndex{}
                               : index_for_object(par);

    beginRemoveRows(parent_idx, r, r);
    auto owned = obj->detach();
    endRemoveRows();

    emit structure_changed();
    return (owned != nullptr);
}

sg::Object *SceneGraphModel::duplicate_object(sg::Object *obj) {
    if (!obj || !_root || obj == _root) return nullptr;

    auto *par = obj->parent();
    if (!par) return nullptr;

    QModelIndex parent_idx = (par == _root)
                               ? QModelIndex{}
                               : index_for_object(par);

    int row = static_cast<int>(par->children_count());
    beginInsertRows(parent_idx, row, row);
    auto &dup = par->add_child(obj->name + " copy");
    dup.set_translation(obj->translation());
    dup.set_rotation(obj->rotation());
    dup.set_scale_factors(obj->scale_factors());
    dup.visible = obj->visible;
    dup.selectable = obj->selectable;
    endInsertRows();

    emit structure_changed();
    return &dup;
}

}// namespace balsa::visualization::qt
