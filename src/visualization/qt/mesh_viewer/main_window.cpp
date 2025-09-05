#include "balsa/visualization/qt/mesh_viewer/main_window.hpp"
#include "balsa/visualization/qt/mesh_viewer/widget.hpp"
#include "balsa/geometry/triangle_mesh/read_obj.hpp"
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QIcon>
#include <QFileDialog>
#include <spdlog/spdlog.h>
#include <zipper/utils/format.hpp>
#include <QToolBar>
#include <QSlider>
#include <QVBoxLayout>
namespace balsa::visualization::qt::mesh_viewer {

MainWindow::MainWindow() {
    m_widget = new Widget(this);

    createMenus();
    setCentralWidget(m_widget);

    QToolBar *bar = new QToolBar(this);
    auto r = new QSlider(Qt::Orientation::Horizontal, this);
    r->setMinimum(0);
    r->setMaximum(255);
    bar->addWidget(r);
    auto g = new QSlider(Qt::Orientation::Horizontal, this);
    g->setMinimum(0);
    g->setMaximum(255);
    bar->addWidget(g);
    auto b = new QSlider(Qt::Orientation::Horizontal, this);
    b->setMinimum(0);
    b->setMaximum(255);
    bar->addWidget(b);

    QVBoxLayout *layout = new QVBoxLayout(this);
    bar->setLayout(layout);


    connect(r, &QSlider::valueChanged, m_widget, &Widget::setR);
    connect(g, &QSlider::valueChanged, m_widget, &Widget::setG);
    connect(b, &QSlider::valueChanged, m_widget, &Widget::setB);

    addToolBar(Qt::ToolBarArea::LeftToolBarArea, bar);
}

MainWindow::~MainWindow() = default;


void MainWindow::createMenus() {
    createFileMenu();
}

void MainWindow::createFileMenu() {
    // Default menu options
    QMenu *file_menu = menuBar()->addMenu(tr("&File"));


    const QIcon openIcon = QIcon::fromTheme("document-open");
    QAction *openAct = new QAction(openIcon, tr("&Open"), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open a mesh"));
    file_menu->addAction(openAct);
    connect(openAct, &QAction::triggered, this, &MainWindow::openMesh);
}


void MainWindow::openMesh() {
    QString fileName = QFileDialog::getOpenFileName(this);
    if (!fileName.isEmpty()) {
        bool success = loadMesh(fileName);
        if (!success) {
            spdlog::error("Failed to load mesh {}", fileName.toStdString());
        }
    }
}

bool MainWindow::loadMesh(const QString &str) {

    spdlog::info("Tried to load {}", str.toStdString());


    std::filesystem::path path = str.toStdString();

    // TODO: mesh type selection
    if (path.extension().string() == ".obj") {
        auto m = balsa::geometry::triangle_mesh::read_objD(path);
        const auto &pos = m.position;
        const RowVectors<float, 3> V = pos.vertices.transpose().cast<float>();
        const RowVectors<GLuint, 3> F = pos.triangles.transpose().cast<GLuint>();
        auto v = V.as_span();
        auto f = F.as_span();
        load(v, f);
    }

    return true;
}

void MainWindow::load(const RowVectors<float, 3>::const_span_type &V, const RowVectors<GLuint, 3>::const_span_type &F) {

    spdlog::info("Vertices\n: {}", V);
    spdlog::info("Faces:\n: {}", F);
    Widget *w = dynamic_cast<Widget *>(this->centralWidget());

    w->setMesh(V, F);
}

}// namespace balsa::visualization::qt::mesh_viewer
