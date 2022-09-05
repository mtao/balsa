#if !defined(BALSA_VISUALIZATION_QT_MESH_VIEWER_WIDGET_HPP)
#define BALSA_VISUALIZATION_QT_MESH_VIEWER_WIDGET_HPP


#include <QtWidgets/QOpenGLWidget>

namespace balsa::visualization::qt::mesh_viewer {
class Widget : public QOpenGLWidget {
  public:
    Widget();
    ~Widget();


    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;
};
}// namespace balsa::visualization::qt::mesh_viewer

#endif
