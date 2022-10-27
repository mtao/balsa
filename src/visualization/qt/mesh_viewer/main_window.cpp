#include "balsa/visualization/qt/mesh_viewer/main_Window.hpp"
#include "balsa/visualization/qt/mesh_viewer/widget.hpp"
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtGui/QIcon>
#include <QtWidgets/QFileDialog>
#include <spdlog/spdlog.h>
namespace balsa::visualization::qt::mesh_viewer {

MainWindow::MainWindow() {
    m_widget = new Widget();

    createMenus();
    setCentralWidget(m_widget);
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
    return true;
}

}// namespace balsa::visualization::qt::mesh_viewer
