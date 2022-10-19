#include <QtWidgets/QApplication>
#include <balsa/visualization/qt/mesh_viewer/main_window.hpp>


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    balsa::visualization::qt::mesh_viewer::MainWindow mw;

    mw.show();
    return app.exec();
}
