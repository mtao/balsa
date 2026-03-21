#include "balsa/visualization/qt/mesh_viewer/widget.hpp"
#include <ranges>
#include <fstream>
#include <sstream>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <spdlog/spdlog.h>
#include <QOpenGLPaintDevice>
#include <QWindow>
#include <QOpenGLShaderProgram>
#include <QWheelEvent>
#include <QMouseEvent>
#include <filesystem>
namespace balsa::visualization::qt::mesh_viewer {
namespace {
    auto get_source(const std::filesystem::path &&name) -> std::string {

        std::filesystem::path source_path = __FILE__;
        source_path = std::filesystem::absolute(source_path);
        std::filesystem::path src = source_path.parent_path() / name;
        std::ifstream ifs(src);
        std::ostringstream oss;
        oss << ifs.rdbuf();
        return oss.str();
    }
    static const std::string vertexShaderSource = get_source("shaders/phong.vert");

    static const std::string fragmentShaderSource = get_source("shaders/phong.frag");
    ;
}// namespace
void Widget::update_mvp() {

    makeCurrent();
    m_program->bind();
    m_mvp_uniform = m_program->uniformLocation("MVP");

    Q_ASSERT(m_mvp_uniform != -1);
    QMatrix4x4 MVP = m_perspective * m_view * m_model;
    // qDebug() << MVP;
    m_program->setUniformValue("MVP", MVP);
    GLint value = m_program->uniformLocation("value");

    // qDebug() << "Value: " << value;
    auto p = m_view.column(3);
    m_program->setUniformValue(value, p.x(), p.y(), p.z());


    m_program->release();
    /*
    char name[10];
    auto &glf = functions();
    GLsizei *length;
    GLint *size;
    GLenum *type;
    glf.glGetActiveUniform(m_program->programId(), value, 10, length, size, type, name);

    qDebug() << name;
    */
    update();
}

auto Widget::functions()
  -> QOpenGLFunctions & {

    return *QOpenGLContext::currentContext()->functions();
}
Widget::Widget(QWidget *parent) : QOpenGLWidget(parent) {
    m_model.setToIdentity();
    m_view.setToIdentity();
    m_view.scale(.5);
    m_view.lookAt({ 0, 0, -1 }, { 0, 0, 0 }, { 0, 1, 0 });
}
Widget::~Widget() = default;
void Widget::initializeGL() {
    QOpenGLWidget::initializeGL();
    qDebug() << "version: "
             << QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    qDebug() << "GLSL version: "
             << QLatin1String(reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION)));


    //// clang-format off
    // static const GLfloat vertices_colors[] = {
    //     /*p=*/+0.0f, +0.707f, /*col=*/1.0f, 0.0f, 0.0f,
    //     /*p=*/-0.5f, -0.500f, /*col=*/0.0f, 1.0f, 0.0f,
    //     /*p=*/+0.5f, -0.500f, /*col=*/0.0f, 0.0f, 1.0f };
    ////clang-format on
    const ColVectors<float, 3> vertices = {
        { +0.0f,
          -0.5f,
          +0.5f },
        { +0.707f,
          -0.500f,
          -0.500f }

        //{ +0.0f, +0.707f, 0.f },
        //{ -0.5f, -0.500f, 0.f },
        //{ +0.5f, -0.500f, 0.f }
    };
    const ColVectors<GLuint, 3> faces = { { 0 }, { 1 }, { 2 } };

    m_vbo.create();
    m_ibo.create();
    setMesh(vertices.as_span(), faces.as_span());

    m_program = new QOpenGLShaderProgram(this);
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource.c_str());
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource.c_str());
    m_program->bindAttributeLocation("posAttr", 0);
    // m_program->bindAttributeLocation("colAttr", 1);
    m_program->link();

    m_program->bind();
    rgb.setRgbF(.5, .5, 0., 1.0);
    m_program->setUniformValue("rgba", 1.f, 1.f, 1.f, 1.f);
    // m_program->setUniformValue("rgba", rgb);
    assert(glGetError() == GL_NO_ERROR);
    m_program->release();

    update_mvp();
    setR(128);
    setG(128);
    setB(128);
}
void Widget::resizeGL(int w, int h) {
    spdlog::info("{}x{}", w, h);
    QOpenGLWidget::resizeGL(w, h);
    const qreal retinaScale = devicePixelRatio();
    m_perspective.setToIdentity();
    m_perspective.perspective(60.0, float(w) / float(h), 1e-3f, 10.f);
    glViewport(0, 0, width() * retinaScale, height() * retinaScale);
    update_mvp();
}
void Widget::paintGL() {
    QOpenGLWidget::paintGL();
    auto &glf = functions();


    glf.glClearColor(0.0, 0.0, 0.0, 1.0);

    glf.glEnable(GL_DEPTH_TEST);
    glf.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // m_program->bind();
    // m_program->setUniformValue("rgba", rgb);
    // m_program->release();

    m_program->bind();
    m_vbo.bind();
    glf.glEnableVertexAttribArray(0);
    m_ibo.bind();
    glf.glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_INT, nullptr);

    m_ibo.release();
    glf.glDisableVertexAttribArray(0);
    m_vbo.release();
    m_program->release();
    // glf.glDisableVertexAttribArray(1);

    // m_program->release();
}
void Widget::setMesh(const ColVectors<float, 2>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles) {
    _setMesh<float, 2>(vertices, triangles);
}
void Widget::setMesh(const ColVectors<float, 3>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles) {
    _setMesh<float, 3>(vertices, triangles);
    //
}

void Widget::setMesh(const ColVectors<double, 2>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles) {
    _setMesh<double, 2>(vertices, triangles);
}
void Widget::setMesh(const ColVectors<double, 3>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles) {
    _setMesh<double, 3>(vertices, triangles);
    //
}

template<typename T, zipper::index_type Dim>
void Widget::_setMesh(const ColVectors<T, Dim>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles) {

    // size = vertices.extent(0);
    size = triangles.extent(0) * triangles.extent(1);
    auto &glf = functions();
    m_vbo.bind();
    auto v = vertices.expression().as_std_span();
    m_vbo.allocate(v.data(), v.size() * sizeof(T));

    glf.glEnableVertexAttribArray(0);
    // glf.glEnableVertexAttribArray(1);

    glf.glVertexAttribPointer(0, Dim, std::is_same_v<double, T> ? GL_DOUBLE : GL_FLOAT, GL_FALSE, Dim * sizeof(GLfloat), nullptr);


    m_vbo.release();
    glf.glDisableVertexAttribArray(0);
    m_ibo.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
    m_ibo.bind();
    // GLuint f[] = { 0, 1, 2 };
    // m_ibo.allocate(f, sizeof(f));
    auto f = triangles.expression().as_std_span();
    m_ibo.allocate(f.data(), f.size() * sizeof(GLuint));

    m_ibo.release();
    update();
}


void Widget::setR(int value) {
    rgb.setRed(value);
    makeCurrent();
    m_program->bind();
    m_program->setUniformValue("rgba", rgb);
    m_program->release();
    update();
}
void Widget::setG(int value) {
    rgb.setGreen(
      value);
    makeCurrent();
    m_program->bind();
    m_program->setUniformValue("rgba", rgb);
    m_program->release();
    update();
}
void Widget::setB(int value) {
    rgb.setBlue(
      value);
    makeCurrent();
    m_program->bind();
    m_program->setUniformValue("rgba", rgb);
    m_program->release();
    update();
}

void Widget::dragEnterEvent(QDragEnterEvent *event) {
    QOpenGLWidget::dragEnterEvent(event);
    qDebug() << "dragEnterEvent()";
}

void Widget::dragLeaveEvent(QDragLeaveEvent *event) {
    QOpenGLWidget::dragLeaveEvent(event);
    qDebug() << "dragLeaveEvent()";
}
void Widget::dragMoveEvent(QDragMoveEvent *event) {
    QOpenGLWidget::dragMoveEvent(event);
    qDebug() << "dragMoveEvent()";
}
void Widget::keyPressEvent(QKeyEvent *event) {
    QOpenGLWidget::keyPressEvent(event);
    qDebug() << "keyPressEvent()";
}
void Widget::keyReleaseEvent(QKeyEvent *event) {
    QOpenGLWidget::keyReleaseEvent(event);
    qDebug() << "keyReleaseEvent()";
}
void Widget::mouseDoubleClickEvent(QMouseEvent *event) {
    QOpenGLWidget::mouseDoubleClickEvent(event);
    qDebug() << "mouseDoubleClickEvent()";
}
void Widget::mouseMoveEvent(QMouseEvent *event) {
    QOpenGLWidget::mouseMoveEvent(event);
    qDebug() << "mouseMoveEvent()";
    QPointF delta = updateMousePosition(event->position());
    qDebug() << delta;

    delta *= -1e-1;

    QVector3D dir(delta.x(), delta.y(), 0.0);


    // m_view.translate(delta.x(), delta.y(), 0.0);
    m_view.rotate(-delta.x(), 0, 1, 0);
    m_view.rotate(delta.y(), 1, 0, 0);
    update_mvp();
}
void Widget::mousePressEvent(QMouseEvent *event) {
    QOpenGLWidget::mousePressEvent(event);
    points ^= true;
    qDebug() << "mousePressEvent()";

    // updateMousePosition(event->position());
    m_lastMousePosition = event->position();
    update();
}

auto Widget::updateMousePosition(const QPointF &p) -> QPointF {
    QPointF delta = p - m_lastMousePosition;
    m_lastMousePosition = p;
    return delta;
}
void Widget::mouseReleaseEvent(QMouseEvent *event) {
    QOpenGLWidget::mouseReleaseEvent(event);
    qDebug() << "mouseReleaseEvent()";
    update_mvp();
}
void Widget::moveEvent(QMoveEvent *event) {
    QOpenGLWidget::moveEvent(event);
    qDebug() << "moveEvent()";
}
void Widget::wheelEvent(QWheelEvent *event) {
    QOpenGLWidget::wheelEvent(event);

    QPoint delta = 1e-1 * event->angleDelta();
    m_view.scale(exp2(1e-3 * delta.y()));
    update_mvp();
    qDebug() << "wheelEvent()" << delta;
}
}// namespace balsa::visualization::qt::mesh_viewer
