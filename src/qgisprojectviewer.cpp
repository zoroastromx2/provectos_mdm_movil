#include "qgisprojectviewer.h"

#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QApplication>
#include <QList>

#include <qgsproject.h>
#include <qgsmapcanvas.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertree.h>
#include <qgslayertreemapcanvasbridge.h>
#include <qgsmaplayer.h>
#include <qgscoordinatereferencesystem.h>

// ---------------------------------------------------------------------------
// QgisProjectViewer
// ---------------------------------------------------------------------------

QgisProjectViewer::QgisProjectViewer(QWidget *parent)
    : QMainWindow(parent)
{
    // QgsProject es un singleton global compartido por todo el proceso.
    m_project = QgsProject::instance();

    setupUi();
    createConnections();
    updateWindowTitle();
}

QgisProjectViewer::~QgisProjectViewer() = default;

void QgisProjectViewer::setupUi()
{
    resize(1200, 800);

    setupCentralWidget();
    setupDockWidgets();
    setupMenuBar();

    m_statusBar = statusBar();
    m_statusBar->showMessage(tr("Listo"));
}

void QgisProjectViewer::setupMenuBar()
{
    // --- Menú "Archivo" -----------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&Archivo"));

    m_actionOpen = fileMenu->addAction(tr("&Abrir proyecto…"));
    m_actionOpen->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();

    m_actionClose = fileMenu->addAction(tr("&Cerrar proyecto"));
    fileMenu->addSeparator();

    m_actionExit = fileMenu->addAction(tr("&Salir"));
    m_actionExit->setShortcut(QKeySequence::Close);

    // --- Menú "Ver" -----------------------------------------------------------
    QMenu *viewMenu = menuBar()->addMenu(tr("&Ver"));

    m_actionZoomFull = viewMenu->addAction(tr("&Zoom a extensión completa"));
    m_actionZoomFull->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    viewMenu->addSeparator();
    viewMenu->addAction(m_layerDock->toggleViewAction());

    // --- Menú "Herramientas" (preparado para futuras funciones) ----------------
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Herramientas"));
    m_actionAbout = toolsMenu->addAction(tr("&Acerca de…"));
}

void QgisProjectViewer::setupDockWidgets()
{
    m_layerDock = new QDockWidget(tr("Capas"), this);
    m_layerDock->setObjectName(QStringLiteral("LayerTreeDock"));
    m_layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_layerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_layerTreeModel = new QgsLayerTreeModel(m_project->layerTreeRoot(), m_layerDock);
    m_layerTreeModel->setFlag(QgsLayerTreeModel::AllowNodeReorder);
    m_layerTreeModel->setFlag(QgsLayerTreeModel::AllowNodeRename);
    m_layerTreeModel->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility);
    m_layerTreeModel->setFlag(QgsLayerTreeModel::ShowLegendAsTree);

    m_layerTreeView = new QgsLayerTreeView(m_layerDock);
    m_layerTreeView->setModel(m_layerTreeModel);

    m_layerDock->setWidget(m_layerTreeView);
    addDockWidget(Qt::LeftDockWidgetArea, m_layerDock);
}

void QgisProjectViewer::setupCentralWidget()
{
    m_mapCanvas = new QgsMapCanvas(this);
    m_mapCanvas->setCanvasColor(Qt::white);
    m_mapCanvas->enableAntiAliasing(true);
    m_mapCanvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")));

    setCentralWidget(m_mapCanvas);
}

void QgisProjectViewer::createConnections()
{
    connect(m_actionOpen,      &QAction::triggered, this, &QgisProjectViewer::onActionOpenProject);
    connect(m_actionClose,     &QAction::triggered, this, &QgisProjectViewer::onActionCloseProject);
    connect(m_actionExit,      &QAction::triggered, this, &QgisProjectViewer::onActionExit);
    connect(m_actionZoomFull,  &QAction::triggered, this, &QgisProjectViewer::onActionZoomFull);
    connect(m_actionAbout,     &QAction::triggered, this, &QgisProjectViewer::onActionAbout);
}

bool QgisProjectViewer::loadProject(const QString &projectPath)
{
    m_lastError.clear();

    const QFileInfo fileInfo(projectPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        m_lastError = tr("El archivo de proyecto no existe: %1").arg(projectPath);
        emit projectLoadError(m_lastError);
        return false;
    }

    closeProject();

    // QgsProject::read() se encarga de descomprimir el .qgz y parsear el XML
    // interno, poblando el árbol de capas (layerTreeRoot) y registrando todas
    // las capas del proyecto en el registro de mapas.
    const bool ok = m_project->read(projectPath);
    if (!ok) {
        m_lastError = tr("No se pudo leer el proyecto QGIS: %1")
                          .arg(m_project->error());
        emit projectLoadError(m_lastError);
        return false;
    }

    m_currentProjectPath = projectPath;

    // Reconstruir el modelo del árbol de capas apuntando a la nueva raíz.
    m_layerTreeModel->setRootGroup(m_project->layerTreeRoot());

    // El "bridge" mantiene sincronizados el árbol de capas y el canvas:
    // el orden, la visibilidad y la adición/eliminación de capas en el árbol
    // se reflejan automáticamente en el canvas.
    delete m_canvasBridge;
    m_canvasBridge = new QgsLayerTreeMapCanvasBridge(m_project->layerTreeRoot(), m_mapCanvas, this);

    m_mapCanvas->setDestinationCrs(m_project->crs());

    const QList<QgsMapLayer *> layers = m_project->mapLayers().values();
    m_mapCanvas->setLayers(layers);

    if (!layers.isEmpty()) {
        m_mapCanvas->zoomToFullExtent();
    }
    m_mapCanvas->refresh();

    updateWindowTitle();
    m_statusBar->showMessage(tr("Proyecto cargado: %1 (%2 capas)")
                                  .arg(fileInfo.fileName())
                                  .arg(layers.size()));

    emit projectLoaded(projectPath);
    return true;
}

QString QgisProjectViewer::lastError() const
{
    return m_lastError;
}

void QgisProjectViewer::updateWindowTitle()
{
    const QString base = tr("Visor de Proyectos QGIS");
    if (m_currentProjectPath.isEmpty()) {
        setWindowTitle(base);
    } else {
        setWindowTitle(QStringLiteral("%1 — %2")
                            .arg(QFileInfo(m_currentProjectPath).fileName(), base));
    }
}

void QgisProjectViewer::closeProject()
{
    if (m_mapCanvas)
        m_mapCanvas->setLayers({});

    m_project->clear();
    m_currentProjectPath.clear();

    if (m_layerTreeModel)
        m_layerTreeModel->setRootGroup(m_project->layerTreeRoot());
}

void QgisProjectViewer::onActionOpenProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Abrir proyecto QGIS"),
        QString(),
        tr("Proyecto QGIS (*.qgz *.qgs)"));

    if (path.isEmpty())
        return;

    if (!loadProject(path)) {
        QMessageBox::critical(this, tr("Error al cargar el proyecto"), m_lastError);
    }
}

void QgisProjectViewer::onActionCloseProject()
{
    closeProject();
    updateWindowTitle();
    m_statusBar->showMessage(tr("Proyecto cerrado"));
}

void QgisProjectViewer::onActionExit()
{
    close();
}

void QgisProjectViewer::onActionZoomFull()
{
    if (m_mapCanvas)
        m_mapCanvas->zoomToFullExtent();
}

void QgisProjectViewer::onActionAbout()
{
    QMessageBox::information(
        this,
        tr("Acerca del visor"),
        tr("Visor de Proyectos QGIS\n\n"
           "Carga y visualiza archivos .qgz/.qgs generados por la aplicación "
           "Proyectos MDM Móvil utilizando la API nativa de QGIS."));
}

// ---------------------------------------------------------------------------
// QgisViewerLauncher
// ---------------------------------------------------------------------------

QgisViewerLauncher::QgisViewerLauncher(QObject *parent)
    : QObject(parent)
{
}

QString QgisViewerLauncher::lastError() const
{
    return m_lastError;
}

void QgisViewerLauncher::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

bool QgisViewerLauncher::openProject(const QString &projectPath)
{
    // Cada ventana se destruye a sí misma al cerrarse; su ciclo de vida no
    // necesita ser gestionado manualmente desde aquí.
    auto *viewer = new QgisProjectViewer();
    viewer->setAttribute(Qt::WA_DeleteOnClose);

    if (!viewer->loadProject(projectPath)) {
        setLastError(viewer->lastError());
        viewer->deleteLater();
        return false;
    }

    setLastError(QString());
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
    return true;
}
