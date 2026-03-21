#include "balsa/visualization/qt/mesh_viewer/main_window.hpp"
#include "balsa/visualization/qt/mesh_viewer/widget.hpp"
#include "balsa/geometry/triangle_mesh/read_obj.hpp"
#include "balsa/geometry/bounding_box.hpp"
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QIcon>
#include <QFileDialog>
#include <spdlog/spdlog.h>
#include <zipper/utils/format.hpp>
#include <zipper/utils/max_coeff.hpp>
#include <QToolBar>
#include <QSlider>
#include <QVBoxLayout>
#include <zipper/Form.hpp>
#include <zipper/expression/nullary/Constant.hpp>
namespace balsa::visualization::qt::mesh_viewer {

MainWindow::MainWindow() {
    m_widget = new Widget(this);

    createMenus();
    setCentralWidget(m_widget);

    auto *bar = new QToolBar(this);
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

    auto *layout = new QVBoxLayout(this);
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
    auto *openAct = new QAction(openIcon, tr("&Open"), this);
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

auto MainWindow::loadMesh(const QString &str) -> bool {

    spdlog::info("Tried to load {}", str.toStdString());


    std::filesystem::path path = str.toStdString();

    // TODO: mesh type selection
    if (path.extension().string() == ".obj") {
        auto m = balsa::geometry::triangle_mesh::read_objD(path);
        const auto &pos = m.position;


        ColVectors<float, 3> V = pos.vertices.cast<float>();
        auto bb = balsa::geometry::bounding_box(V);
        assert(bb.min().size() == 3);
        assert(bb.max().size() == 3);
        // TODO: spdlog/fmt version mismatch prevents formatting zipper types
        // spdlog::info("Input {} {} => {}", bb.min(), bb.max(), bb.range());

        double r = ::zipper::utils::maxCoeff(bb.range());

        Vector3<float> center = (bb.min() + bb.max()) / 2.0;
        for (::zipper::index_type j = 0; j < V.extent(1); ++j) {
            auto mv = V.col(j);
            assert(mv.size() == 3);
            mv = (mv - center) / r;
        }
        /*
        bb = balsa::geometry::bounding_box(V);
        spdlog::info("{} {} => {}", bb.min(), bb.max(), bb.range());
        */
        /*
        auto fbb = balsa::geometry::bounding_box(pos.triangles);


        assert(fbb.min().size() == 3);
        assert(fbb.max().size() == 3);
        for(rank_type j = 0; j < 3; ++j) {
            assert(fbb.min()(j) >= 0);
            assert(fbb.max()(j) < V.rows());
        }
        */

        const ColVectors<GLuint, 3> F = pos.triangles.cast<GLuint>();

        // spdlog::info("Faces:\n: {}", pos.triangles);
        auto v = V.as_span();
        auto f = F.as_span();
        load(v, f);
    }

    return true;
}

void MainWindow::load(const ColVectors<float, 3>::const_span_type &V, const ColVectors<GLuint, 3>::const_span_type &F) {

    // spdlog::info("Vertices\n: {}", V);
    // spdlog::info("Faces:\n: {}", F);
    auto *w = dynamic_cast<Widget *>(this->centralWidget());

    w->setMesh(V, F);
}

}// namespace balsa::visualization::qt::mesh_viewer
