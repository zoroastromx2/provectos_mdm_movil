#pragma once

//#include <QMainWindow>
#include <QString>
//#include <QTreeView>
//#include <QDockWidget>
//#include <QMenuBar>
//#include <QMenu>
#include <QAction>
//#include <QVBoxLayout>
//#include <QStatusBar>

// Forward declarations de QGIS
class QgsProject;
class QgsMapCanvas;
class QgsLayerTreeView;
class QgsLayerTreeModel;
class QgsLayerTreeMapCanvasBridge;

/**
 * @brief QgisProjectViewer es una ventana nativa Qt (QMainWindow) que carga
 *        y visualiza un proyecto QGIS (.qgz/.qgs) usando la API de QGIS.
 *
 * Arquitectura:
 *   - Menu Bar: menús "Archivo", "Ver", "Herramientas" (extensibles)
 *   - Dock Widget (izquierda): QgsLayerTreeView mostrando la jerarquía de capas
 *   - Central Widget: QgsMapCanvas renderizando las capas del proyecto
 *
 * Uso desde C++:
 * @code
 *   auto *viewer = new QgisProjectViewer();
 *   viewer->loadProject("/path/to/project.qgz");
 *   viewer->show();
 * @endcode
 */
class QgisProjectViewer : public QMainWindow
{
    Q_OBJECT

public:
    explicit QgisProjectViewer(QWidget *parent = nullptr);
    ~QgisProjectViewer() override;

    /**
     * @brief Carga un archivo de proyecto QGIS (.qgz o .qgs).
     *
     * @param projectPath Ruta absoluta al archivo de proyecto.
     * @return true si la carga fue exitosa, false en caso contrario.
     */
    bool loadProject(const QString &projectPath);

    /**
     * @brief Retorna el último error ocurrido durante la carga del proyecto.
     */
    QString lastError() const;

signals:
    /**
     * @brief Emitido cuando un proyecto se carga exitosamente.
     * @param projectPath Ruta del proyecto cargado.
     */
    void projectLoaded(const QString &projectPath);

    /**
     * @brief Emitido cuando ocurre un error al cargar el proyecto.
     * @param error Descripción del error.
     */
    void projectLoadError(const QString &error);

private slots:
    void onActionOpenProject();
    void onActionCloseProject();
    void onActionExit();
    void onActionZoomFull();
    void onActionAbout();

private:
    void setupUi();
    void setupMenuBar();
    void setupDockWidgets();
    void setupCentralWidget();
    void createConnections();
    void updateWindowTitle();
    void closeProject();

    // QGIS components
    QgsProject                  *m_project{nullptr};
    QgsMapCanvas                *m_mapCanvas{nullptr};
    QgsLayerTreeView            *m_layerTreeView{nullptr};
    QgsLayerTreeModel           *m_layerTreeModel{nullptr};
    QgsLayerTreeMapCanvasBridge *m_canvasBridge{nullptr};

    // Qt Widgets
    QDockWidget *m_layerDock{nullptr};
    QStatusBar  *m_statusBar{nullptr};

    // Menu actions
    QAction *m_actionOpen{nullptr};
    QAction *m_actionClose{nullptr};
    QAction *m_actionExit{nullptr};
    QAction *m_actionZoomFull{nullptr};
    QAction *m_actionAbout{nullptr};

    // State
    QString m_lastError;
    QString m_currentProjectPath;
};
