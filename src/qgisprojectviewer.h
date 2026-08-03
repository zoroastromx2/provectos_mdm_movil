#pragma once

#include <QMainWindow>
#include <QString>
#include <QAction>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QDockWidget>
#include <QObject>
#include <QtQml/qqmlregistration.h>

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

/**
 * @brief QgisViewerLauncher es un puente QML -> Widgets que permite abrir
 *        ventanas QgisProjectViewer (QMainWindow) desde código QML/QtQuick.
 *
 * QtQuick no puede instanciar QWidget directamente, por lo que este objeto
 * se registra como singleton invocable en el módulo QML "App" y se encarga
 * de crear, mantener y mostrar las ventanas del visor.
 *
 * Uso desde QML:
 * @code
 *   QgisViewerLauncher.openProject("/ruta/al/proyecto.qgz")
 * @endcode
 */
class QgisViewerLauncher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit QgisViewerLauncher(QObject *parent = nullptr);

    QString lastError() const;

    /**
     * @brief Abre una nueva ventana visor cargando el proyecto indicado.
     * @param projectPath Ruta absoluta al archivo .qgz/.qgs.
     * @return true si la ventana se creó y el proyecto se cargó correctamente.
     */
    Q_INVOKABLE bool openProject(const QString &projectPath);

signals:
    void lastErrorChanged();

private:
    void setLastError(const QString &error);

    QString m_lastError;
};
