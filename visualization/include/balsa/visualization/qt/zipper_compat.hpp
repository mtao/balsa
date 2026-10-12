#pragma once

// ============================================================================
// Qt <-> zipper conversions
// ============================================================================
//
// QMatrix4x4 stores 16 contiguous floats in column-major order, so it can be
// viewed without copying (as_zipper).  QVector2D/3D/4D, QColor and QPointF
// expose no stable data pointer, so they convert by value.

#include <QColor>
#include <QDoubleSpinBox>
#include <QMatrix4x4>
#include <QPointF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>
#include <zipper/storage/layout_types.hpp>

namespace balsa::visualization::qt {

// ── Matrices (non-owning views) ─────────────────────────────────────

using QMatrix4x4View = ::zipper::Matrix<float, 4, 4, false>::span_type;
using QMatrix4x4ConstView = ::zipper::Matrix<float, 4, 4, false>::const_span_type;

/// Mutable zipper view onto a QMatrix4x4's storage (column-major).
/// Note: writing through the view bypasses QMatrix4x4's internal type flags;
/// call m.optimize() afterwards if you modify it this way.
inline auto as_zipper(QMatrix4x4 &m) -> QMatrix4x4View {
    return QMatrix4x4View(m.data());
}
inline auto as_zipper(const QMatrix4x4 &m) -> QMatrix4x4ConstView {
    return QMatrix4x4ConstView(m.constData());
}

/// Copy any 4x4 zipper matrix (or transform) into a QMatrix4x4.
template <::zipper::concepts::Matrix M>
auto to_qt(const M &m) -> QMatrix4x4 {
    QMatrix4x4 q;
    as_zipper(q) = m;
    q.optimize();
    return q;
}
/// Any 3D zipper transform (anything with to_matrix() -> 4x4).
template <typename Xf>
    requires requires(const Xf &xf) { xf.to_matrix(); }
auto to_qt(const Xf &xf) -> QMatrix4x4 {
    return to_qt(xf.to_matrix());
}

// ── Vectors (by value) ──────────────────────────────────────────────

inline auto as_zipper(const QVector2D &v) -> ::zipper::Vector<float, 2> {
    return { v.x(), v.y() };
}
inline auto as_zipper(const QVector3D &v) -> ::zipper::Vector<float, 3> {
    return { v.x(), v.y(), v.z() };
}
inline auto as_zipper(const QVector4D &v) -> ::zipper::Vector<float, 4> {
    return { v.x(), v.y(), v.z(), v.w() };
}
inline auto as_zipper(const QPointF &p) -> ::zipper::Vector<double, 2> {
    return { p.x(), p.y() };
}

template <::zipper::concepts::Vector V>
auto to_qvector3d(const V &v) -> QVector3D {
    return { static_cast<float>(v(0)), static_cast<float>(v(1)),
             static_cast<float>(v(2)) };
}
template <::zipper::concepts::Vector V>
auto to_qvector4d(const V &v) -> QVector4D {
    return { static_cast<float>(v(0)), static_cast<float>(v(1)),
             static_cast<float>(v(2)), static_cast<float>(v(3)) };
}

// ── Colors (RGBA in [0,1]) ──────────────────────────────────────────

inline auto as_zipper(const QColor &c) -> ::zipper::Vector<float, 4> {
    return { static_cast<float>(c.redF()), static_cast<float>(c.greenF()),
             static_cast<float>(c.blueF()), static_cast<float>(c.alphaF()) };
}

/// 3-vectors give an opaque color; 4-vectors include alpha.
template <::zipper::concepts::Vector V>
auto to_qcolor(const V &c) -> QColor {
    const float a = c.extent(0) > 3 ? static_cast<float>(c(3)) : 1.0f;
    return QColor::fromRgbF(static_cast<float>(c(0)), static_cast<float>(c(1)),
                            static_cast<float>(c(2)), a);
}

// ── Widgets ─────────────────────────────────────────────────────────

inline auto read_vec3(const QDoubleSpinBox *x, const QDoubleSpinBox *y,
                      const QDoubleSpinBox *z) -> ::zipper::Vector<float, 3> {
    return { static_cast<float>(x->value()), static_cast<float>(y->value()),
             static_cast<float>(z->value()) };
}

} // namespace balsa::visualization::qt
