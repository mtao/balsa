#if !defined(BALSA_VISUALIZATION_QT_MESH_VIEWER_WIDGET_HPP)
#define BALSA_VISUALIZATION_QT_MESH_VIEWER_WIDGET_HPP


#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <balsa/tensor_types.hpp>
#include <QMatrix4x4>


class QOpenGLShaderProgram;

namespace balsa::visualization::qt::mesh_viewer {
class Widget : public QOpenGLWidget {
  public:
    Widget(QWidget *parent = nullptr);
    ~Widget();


    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;

    void setMesh(const ColVectors<double, 2>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles);
    void setMesh(const ColVectors<double, 3>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles);

    void setMesh(const ColVectors<float, 2>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles);
    void setMesh(const ColVectors<float, 3>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles);


    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;


    void wheelEvent(QWheelEvent *event) override;

  public slots:
    void setR(int value);
    void setG(int value);
    void setB(int value);

  protected:
    auto functions() -> QOpenGLFunctions &;
    void update_mvp();


  private:
    template<typename T, zipper::index_type D>
    void _setMesh(const ColVectors<T, D>::const_span_type &vertices, const ColVectors<GLuint, 3>::const_span_type &triangles);
    QOpenGLBuffer m_vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    QOpenGLBuffer m_ibo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    QOpenGLShaderProgram *m_program = nullptr;
    GLuint size = 0;
    QMatrix4x4 m_model;
    QMatrix4x4 m_view;
    QMatrix4x4 m_perspective;

    QPointF m_lastMousePosition;
    // returns delta
    QPointF updateMousePosition(const QPointF &p);

    GLint m_mvp_uniform = -1;
    bool points;
    QColor rgb;

    Q_OBJECT
};
}// namespace balsa::visualization::qt::mesh_viewer

#endif
