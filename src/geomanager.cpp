#include "geomanager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtConcurrent>

// GDAL / OGR headers
#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogrsf_frmts.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// RAII wrapper so we never forget GDALClose().
struct GdalDatasetGuard
{
    explicit GdalDatasetGuard(GDALDataset *ds) : m_ds(ds) {}
    ~GdalDatasetGuard() { if (m_ds) GDALClose(m_ds); }

    GDALDataset *get()  const { return m_ds; }
    bool isValid()      const { return m_ds != nullptr; }

    GdalDatasetGuard(const GdalDatasetGuard &)            = delete;
    GdalDatasetGuard &operator=(const GdalDatasetGuard &) = delete;

private:
    GDALDataset *m_ds;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

GeoManager::GeoManager(QObject *parent)
    : QObject(parent)
{
    GDALAllRegister();
}

GeoManager::~GeoManager()
{
    // Clean up any remaining watchers
    if (m_createGpkgWatcher) {
        m_createGpkgWatcher->waitForFinished();
        delete m_createGpkgWatcher;
    }
    if (m_openGpkgWatcher) {
        m_openGpkgWatcher->waitForFinished();
        delete m_openGpkgWatcher;
    }
    if (m_addLayersWatcher) {
        m_addLayersWatcher->waitForFinished();
        delete m_addLayersWatcher;
    }
    if (m_removeLayerWatcher) {
        m_removeLayerWatcher->waitForFinished();
        delete m_removeLayerWatcher;
    }
}

// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

QString GeoManager::activeGpkgPath() const { return m_activeGpkgPath; }
QStringList GeoManager::layerNames() const { return m_layerNames; }
QString GeoManager::lastError()      const { return m_lastError; }
bool    GeoManager::busy()           const { return m_busy; }

void GeoManager::setActiveGpkgPath(const QString &path)
{
    if (m_activeGpkgPath == path)
        return;
    m_activeGpkgPath = path;
    emit activeGpkgPathChanged();
}

void GeoManager::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void GeoManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QString GeoManager::urlToPath(const QString &urlString)
{
    // QML FileDialog returns "file:///C:/path/to/file" on Windows and
    // "file:///home/user/file" on Linux.  QUrl handles both correctly.
    QUrl url(urlString);
    if (url.isLocalFile())
        return url.toLocalFile();
    return urlString; // already a plain path
}

QStringList GeoManager::urlListToPaths(const QStringList &urlList)
{
    QStringList paths;
    paths.reserve(urlList.size());
    for (const QString &u : urlList)
        paths << urlToPath(u);
    return paths;
}

// ---------------------------------------------------------------------------
// Internal: reload layer names from disk
// ---------------------------------------------------------------------------

bool GeoManager::reloadLayerNames()
{
    if (m_activeGpkgPath.isEmpty()) {
        m_layerNames.clear();
        emit layerNamesChanged();
        return true;
    }

    GdalDatasetGuard ds{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};

    if (!ds.isValid()) {
        setLastError(tr("No se puede abrir el GeoPackage: %1").arg(m_activeGpkgPath));
        return false;
    }

    QStringList names;
    const int count = ds.get()->GetLayerCount();
    names.reserve(count);
    for (int i = 0; i < count; ++i) {
        OGRLayer *layer = ds.get()->GetLayer(i);
        if (layer)
            names << QString::fromUtf8(layer->GetName());
    }

    m_layerNames = names;
    emit layerNamesChanged();
    return true;
}

// ---------------------------------------------------------------------------
// GPKG info queries
// ---------------------------------------------------------------------------

QVariantMap GeoManager::getGpkgFileInfo()
{
    QVariantMap info;
    if (m_activeGpkgPath.isEmpty()) {
        info["error"] = tr("No hay un GeoPackage abierto.");
        return info;
    }

    QFileInfo fi(m_activeGpkgPath);
    info["path"]         = QDir::toNativeSeparators(m_activeGpkgPath);
    info["sizeBytes"]    = static_cast<qlonglong>(fi.size());
    info["lastModified"] = fi.lastModified().toString(Qt::ISODate);

    GdalDatasetGuard ds{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};
    info["totalLayers"] = ds.isValid()
        ? static_cast<int>(ds.get()->GetLayerCount()) : 0;

    return info;
}

QVariantList GeoManager::getAllLayerInfo()
{
    QVariantList layers;
    if (m_activeGpkgPath.isEmpty())
        return layers;

    GdalDatasetGuard ds{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};
    if (!ds.isValid())
        return layers;

    const int count = ds.get()->GetLayerCount();
    for (int i = 0; i < count; ++i) {
        OGRLayer *layer = ds.get()->GetLayer(i);
        if (!layer)
            continue;

        QVariantMap l;
        l["name"]         = QString::fromUtf8(layer->GetName());
        l["featureCount"] = static_cast<qlonglong>(layer->GetFeatureCount(true));

        // Geometry type
        l["geomType"] = QString::fromUtf8(
            OGRToOGCGeomType(layer->GetGeomType()));

        // CRS
        const OGRSpatialReference *srs = layer->GetSpatialRef();
        if (srs) {
            const char *authName = srs->GetAuthorityName(nullptr);
            const char *authCode = srs->GetAuthorityCode(nullptr);
            if (authName && authCode)
                l["crsAuth"] = QString::fromUtf8(authName) + QLatin1Char(':')
                               + QString::fromUtf8(authCode);

            char *pszWkt = nullptr;
            srs->exportToWkt(&pszWkt);
            if (pszWkt) {
                l["crsWkt"] = QString::fromUtf8(pszWkt);
                CPLFree(pszWkt);
            }
        }

        // Extent
        OGREnvelope env;
        if (layer->GetExtent(&env, true) == OGRERR_NONE) {
            l["minX"] = env.MinX;
            l["minY"] = env.MinY;
            l["maxX"] = env.MaxX;
            l["maxY"] = env.MaxY;
        }

        layers.append(l);
    }

    return layers;
}

// ---------------------------------------------------------------------------
// Public API (async versions using QtConcurrent)
// ---------------------------------------------------------------------------

bool GeoManager::createGeoPackage(const QStringList &shpPaths,
                                   const QString     &gpkgPath)
{
    if (gpkgPath.isEmpty()) {
        setLastError(tr("La ruta del GeoPackage está vacía."));
        return false;
    }
    if (shpPaths.isEmpty()) {
        setLastError(tr("No hay ruta del archivo shape."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    // Clean up any previous watcher
    if (m_createGpkgWatcher) {
        m_createGpkgWatcher->waitForFinished();
        delete m_createGpkgWatcher;
    }

    // Create a new watcher and connect it
    m_createGpkgWatcher = new QFutureWatcher<CreateGpkgResult>(this);
    connect(m_createGpkgWatcher, &QFutureWatcher<CreateGpkgResult>::finished,
            this, &GeoManager::onCreateGeoPackageFinished);

    // Launch the worker method in a thread pool
    QFuture<CreateGpkgResult> future = QtConcurrent::run(
        [this, shpPaths, gpkgPath]() {
            return createGeoPackageWorker(shpPaths, gpkgPath);
        });

    m_createGpkgWatcher->setFuture(future);
    return true; // Always return true; result comes via signal
}

bool GeoManager::openGeoPackage(const QString &gpkgPath)
{
    if (gpkgPath.isEmpty()) {
        setLastError(tr("La ruta del GeoPackage está vacía."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    // Clean up any previous watcher
    if (m_openGpkgWatcher) {
        m_openGpkgWatcher->waitForFinished();
        delete m_openGpkgWatcher;
    }

    // Create a new watcher and connect it
    m_openGpkgWatcher = new QFutureWatcher<OpenGpkgResult>(this);
    connect(m_openGpkgWatcher, &QFutureWatcher<OpenGpkgResult>::finished,
            this, &GeoManager::onOpenGeoPackageFinished);

    // Launch the worker method in a thread pool
    QFuture<OpenGpkgResult> future = QtConcurrent::run(
        [this, gpkgPath]() {
            return openGeoPackageWorker(gpkgPath);
        });

    m_openGpkgWatcher->setFuture(future);
    return true; // Always return true; result comes via signal
}

bool GeoManager::addLayers(const QStringList &shpPaths)
{
    if (m_activeGpkgPath.isEmpty()) {
        setLastError(tr("No hay un GeoPackage abierto actualmente."));
        return false;
    }
    if (shpPaths.isEmpty()) {
        setLastError(tr("No hay ruta para el shape."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    // Clean up any previous watcher
    if (m_addLayersWatcher) {
        m_addLayersWatcher->waitForFinished();
        delete m_addLayersWatcher;
    }

    // Create a new watcher and connect it
    m_addLayersWatcher = new QFutureWatcher<AddLayersResult>(this);
    connect(m_addLayersWatcher, &QFutureWatcher<AddLayersResult>::finished,
            this, &GeoManager::onAddLayersFinished);

    // Launch the worker method in a thread pool
    QFuture<AddLayersResult> future = QtConcurrent::run(
        [this, shpPaths]() {
            return addLayersWorker(shpPaths);
        });

    m_addLayersWatcher->setFuture(future);
    return true; // Always return true; result comes via signal
}

bool GeoManager::removeLayer(const QString &layerName)
{
    if (m_activeGpkgPath.isEmpty()) {
        setLastError(tr("No hay un GeoPackage abierto actualmente."));
        return false;
    }
    if (layerName.isEmpty()) {
        setLastError(tr("El nombre de la caoa está vacía."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    // Clean up any previous watcher
    if (m_removeLayerWatcher) {
        m_removeLayerWatcher->waitForFinished();
        delete m_removeLayerWatcher;
    }

    // Create a new watcher and connect it
    m_removeLayerWatcher = new QFutureWatcher<RemoveLayerResult>(this);
    connect(m_removeLayerWatcher, &QFutureWatcher<RemoveLayerResult>::finished,
            this, &GeoManager::onRemoveLayerFinished);

    // Launch the worker method in a thread pool
    QFuture<RemoveLayerResult> future = QtConcurrent::run(
        [this, layerName]() {
            return removeLayerWorker(layerName);
        });

    m_removeLayerWatcher->setFuture(future);
    return true; // Always return true; result comes via signal
}

void GeoManager::refreshLayers()
{
    // For refresh, we can use the async open method
    if (!m_activeGpkgPath.isEmpty()) {
        openGeoPackage(m_activeGpkgPath);
    }
}

// ---------------------------------------------------------------------------
// Worker methods (run in background thread)
// ---------------------------------------------------------------------------

GeoManager::CreateGpkgResult GeoManager::createGeoPackageWorker(
    const QStringList &shpPaths, const QString &gpkgPath)
{
    CreateGpkgResult result;
    result.gpkgPath = gpkgPath;
    result.success = false;
    result.errorMsg.clear();

    // Get (or create) the GPKG driver
    GDALDriver *gpkgDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if (!gpkgDriver) {
        result.errorMsg = tr("El controlador GPKG no está disponible. Checa la instalación del GDAL.");
        return result;
    }

    // Create the new (empty) GeoPackage – truncating any existing file
    GdalDatasetGuard outDS{gpkgDriver->Create(
        gpkgPath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr)};

    if (!outDS.isValid()) {
        result.errorMsg = tr("Falla al crear el GeoPackage: %1").arg(gpkgPath);
        return result;
    }

    bool anyImported = false;
    for (const QString &shpPath : shpPaths) {
        GdalDatasetGuard srcDS{static_cast<GDALDataset *>(
            GDALOpenEx(shpPath.toUtf8().constData(),
                       GDAL_OF_VECTOR | GDAL_OF_READONLY,
                       nullptr, nullptr, nullptr))};

        if (!srcDS.isValid()) {
            result.errorMsg = tr("No se puede abrir el archivo shape: %1 – saltado.").arg(shpPath);
            continue;
        }

        const QString layerName = QFileInfo(shpPath).baseName();

        for (int i = 0; i < srcDS.get()->GetLayerCount(); ++i) {
            OGRLayer *srcLayer = srcDS.get()->GetLayer(i);
            if (!srcLayer)
                continue;

            OGRLayer *newLayer = outDS.get()->CopyLayer(srcLayer,
                                                        layerName.toUtf8().constData(),
                                                        nullptr);
            if (!newLayer) {
                result.errorMsg = tr("Failed to copy layer '%1' from '%2'.")
                                 .arg(layerName, shpPath);
            } else {
                anyImported = true;
            }
        }
    }

    if (!anyImported) {
        result.errorMsg = tr("No fueron importadas capas dentro del GeoPackage.");
        return result;
    }

    // Read layer names from the newly created GPKG
    GdalDatasetGuard readDS{static_cast<GDALDataset *>(
        GDALOpenEx(gpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};
    if (readDS.isValid()) {
        const int count = readDS.get()->GetLayerCount();
        for (int i = 0; i < count; ++i) {
            OGRLayer *layer = readDS.get()->GetLayer(i);
            if (layer)
                result.layerNames << QString::fromUtf8(layer->GetName());
        }
    }

    result.success = true;
    return result;
}

GeoManager::OpenGpkgResult GeoManager::openGeoPackageWorker(const QString &gpkgPath)
{
    OpenGpkgResult result;
    result.gpkgPath = gpkgPath;
    result.success = false;
    result.errorMsg.clear();

    GdalDatasetGuard ds{static_cast<GDALDataset *>(
        GDALOpenEx(gpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};

    if (!ds.isValid()) {
        result.errorMsg = tr("No se puede abrir el GeoPackage: %1").arg(gpkgPath);
        return result;
    }

    const int count = ds.get()->GetLayerCount();
    for (int i = 0; i < count; ++i) {
        OGRLayer *layer = ds.get()->GetLayer(i);
        if (layer)
            result.layerNames << QString::fromUtf8(layer->GetName());
    }

    result.success = true;
    return result;
}

GeoManager::AddLayersResult GeoManager::addLayersWorker(const QStringList &shpPaths)
{
    AddLayersResult result;
    result.success = false;
    result.errorMsg.clear();

    GdalDatasetGuard outDS{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                   nullptr, nullptr, nullptr))};

    if (!outDS.isValid()) {
        result.errorMsg = tr("No se puede abrir el GeoPackage para edición: %1")
                         .arg(m_activeGpkgPath);
        return result;
    }

    bool anyAdded = false;
    for (const QString &shpPath : shpPaths) {
        GdalDatasetGuard srcDS{static_cast<GDALDataset *>(
            GDALOpenEx(shpPath.toUtf8().constData(),
                       GDAL_OF_VECTOR | GDAL_OF_READONLY,
                       nullptr, nullptr, nullptr))};

        if (!srcDS.isValid()) {
            result.errorMsg = tr("No se puede abrir el shape: %1 – brincado.").arg(shpPath);
            continue;
        }

        const QString layerName = QFileInfo(shpPath).baseName();

        for (int i = 0; i < srcDS.get()->GetLayerCount(); ++i) {
            OGRLayer *srcLayer = srcDS.get()->GetLayer(i);
            if (!srcLayer)
                continue;

            OGRLayer *newLayer = outDS.get()->CopyLayer(srcLayer,
                                                        layerName.toUtf8().constData(),
                                                        nullptr);
            if (!newLayer) {
                result.errorMsg = tr("Hay falla para agregar la capa '%1'.").arg(layerName);
            } else {
                anyAdded = true;
            }
        }
    }

    // Read updated layer names
    GdalDatasetGuard readDS{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};
    if (readDS.isValid()) {
        const int count = readDS.get()->GetLayerCount();
        for (int i = 0; i < count; ++i) {
            OGRLayer *layer = readDS.get()->GetLayer(i);
            if (layer)
                result.layerNames << QString::fromUtf8(layer->GetName());
        }
    }

    result.success = anyAdded;
    return result;
}

GeoManager::RemoveLayerResult GeoManager::removeLayerWorker(const QString &layerName)
{
    RemoveLayerResult result;
    result.success = false;
    result.errorMsg.clear();

    GdalDatasetGuard ds{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                   nullptr, nullptr, nullptr))};

    if (!ds.isValid()) {
        result.errorMsg = tr("No se puede abrir el GeoPackage para edición: %1")
                         .arg(m_activeGpkgPath);
        return result;
    }

    int targetIndex = -1;
    const int count = ds.get()->GetLayerCount();
    for (int i = 0; i < count; ++i) {
        OGRLayer *layer = ds.get()->GetLayer(i);
        if (layer && QString::fromUtf8(layer->GetName()) == layerName) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex < 0) {
        result.errorMsg = tr("La capa '%1' no se encuentra en el GeoPackage.").arg(layerName);
        return result;
    }

    const OGRErr err = ds.get()->DeleteLayer(targetIndex);
    if (err != OGRERR_NONE) {
        result.errorMsg = tr("Fallo al borrar la capa '%1': error en el GDAL %2.")
                         .arg(layerName).arg(static_cast<int>(err));
        return result;
    }

    // Read updated layer names
    GdalDatasetGuard readDS{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};
    if (readDS.isValid()) {
        const int newCount = readDS.get()->GetLayerCount();
        for (int i = 0; i < newCount; ++i) {
            OGRLayer *layer = readDS.get()->GetLayer(i);
            if (layer)
                result.layerNames << QString::fromUtf8(layer->GetName());
        }
    }

    result.success = true;
    return result;
}

// ---------------------------------------------------------------------------
// Completion handlers (run on UI thread)
// ---------------------------------------------------------------------------

void GeoManager::onCreateGeoPackageFinished()
{
    if (!m_createGpkgWatcher)
        return;

    const CreateGpkgResult result = m_createGpkgWatcher->result();

    if (result.success) {
        setActiveGpkgPath(result.gpkgPath);
        m_layerNames = result.layerNames;
        emit layerNamesChanged();
        setLastError(QString{});
        emit operationSucceeded(tr("GeoPackage creado: %1").arg(result.gpkgPath));
    } else {
        setLastError(result.errorMsg);
    }

    setBusy(false);
}

void GeoManager::onOpenGeoPackageFinished()
{
    if (!m_openGpkgWatcher)
        return;

    const OpenGpkgResult result = m_openGpkgWatcher->result();

    if (result.success) {
        setActiveGpkgPath(result.gpkgPath);
        m_layerNames = result.layerNames;
        emit layerNamesChanged();
        setLastError(QString{});
        emit operationSucceeded(tr("Abierto: %1").arg(result.gpkgPath));
    } else {
        setLastError(result.errorMsg);
    }

    setBusy(false);
}

void GeoManager::onAddLayersFinished()
{
    if (!m_addLayersWatcher)
        return;

    const AddLayersResult result = m_addLayersWatcher->result();

    if (result.success) {
        m_layerNames = result.layerNames;
        emit layerNamesChanged();
        setLastError(QString{});
        emit operationSucceeded(tr("Capas agregadas exitósamente."));
    } else {
        setLastError(result.errorMsg);
    }

    setBusy(false);
}

void GeoManager::onRemoveLayerFinished()
{
    if (!m_removeLayerWatcher)
        return;

    const RemoveLayerResult result = m_removeLayerWatcher->result();

    if (result.success) {
        m_layerNames = result.layerNames;
        emit layerNamesChanged();
        setLastError(QString{});
        emit operationSucceeded(tr("Capa removida exitósamente."));
    } else {
        setLastError(result.errorMsg);
    }

    setBusy(false);
}
