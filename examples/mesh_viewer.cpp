#include <QtWidgets/QApplication>
#include <balsa/visualization/qt/mesh_viewer/main_window.hpp>
#include <QSurface>
#include <QSurfaceFormat>


int main(int argc, char *argv[]) {

    QSurfaceFormat format ;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::OpenGLContextProfile::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);

    balsa::visualization::qt::mesh_viewer::MainWindow mw;
    const auto& args = app.arguments();

    mw.show();
    if(args.size() > 1) {
        mw.loadMesh(args.at(1));
    }
    return app.exec();
}
