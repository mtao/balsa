#include "balsa/visualization/qt/mesh_viewer/widget.hpp"
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <spdlog/spdlog.h>
namespace balsa::visualization::qt::mesh_viewer {

Widget::Widget() {
}
Widget::~Widget() = default;
void Widget::initializeGL() {
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    QOpenGLWidget::initializeGL();

    f->glClearColor(1.0, 0.0, 0.0, 0.0);
}
void Widget::resizeGL(int w, int h) {
    spdlog::info("{}x{}", w, h);
    QOpenGLWidget::resizeGL(w, h);
}
void Widget::paintGL() {
    QOpenGLWidget::paintGL();
}
}// namespace balsa::visualization::qt::mesh_viewer
