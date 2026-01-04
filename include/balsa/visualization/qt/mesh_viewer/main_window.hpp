#if !defined(BALSA_VISUALIZATION_QT_MESH_VIEWER_MAIN_WINDOW_HPP)
#define BALSA_VISUALIZATION_QT_MESH_VIEWER_MAIN_WINDOW_HPP

#include <QMainWindow>
#include <balsa/tensor_types.hpp>

namespace balsa::visualization::qt::mesh_viewer {
class Widget;
class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    MainWindow();
    ~MainWindow();

    virtual void createMenus();
    virtual void createFileMenu();

  private:
    Widget *m_widget = nullptr;


  public slots:
    void openMesh();
    bool loadMesh(const QString &path);

  private:
    void load(const ColVectors<float, 3>::const_span_type &vertices, const ColVectors<uint32_t, 3>::const_span_type &triangles);
};
}// namespace balsa::visualization::qt::mesh_viewer

#endif
