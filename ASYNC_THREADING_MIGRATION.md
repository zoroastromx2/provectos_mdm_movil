# Migración a Operaciones Asincrónicas con QtConcurrent

## Resumen de cambios

Se han migrado todas las operaciones GDAL pesadas (crear, abrir, agregar y eliminar capas en GeoPackage) de ejecución sincrónica en el hilo UI a ejecución asincrónica en un pool de hilos de trabajo utilizando `QtConcurrent::run()`.

**Beneficio clave:** La interfaz de usuario ya no se bloquea durante operaciones de carga/procesamiento de archivos SHP grandes.

## Archivos modificados

### 1. `src/geomanager.h`
- **Agregadas:** Include de `<QFutureWatcher>` para monitoreo de tareas asincrónicas
- **Agregadas:** Estructuras de resultado (`CreateGpkgResult`, `OpenGpkgResult`, `AddLayersResult`, `RemoveLayerResult`)
- **Agregadas:** Métodos worker privados para ejecución en hilo
- **Agregadas:** Slots de finalización para procesamiento de resultados en el hilo UI
- **Agregados:** Miembros `QFutureWatcher*` para cada operación

### 2. `src/geomanager.cpp`
- **Incluida:** Header `<QtConcurrent>` para manejo de tareas paralelas
- **Modificados:** Métodos públicos (`createGeoPackage()`, `openGeoPackage()`, `addLayers()`, `removeLayer()`)
  - Ahora lanzan la operación en hilo de trabajo y devuelven inmediatamente
  - No más bloqueo del UI

- **Agregados:** Métodos worker privados
  - `createGeoPackageWorker()`: Crea GPKG en hilo de trabajo
  - `openGeoPackageWorker()`: Abre GPKG en hilo de trabajo
  - `addLayersWorker()`: Agrega capas en hilo de trabajo
  - `removeLayerWorker()`: Elimina capa en hilo de trabajo

- **Agregados:** Handlers de finalización (slots)
  - `onCreateGeoPackageFinished()`: Procesa resultado de creación
  - `onOpenGeoPackageFinished()`: Procesa resultado de apertura
  - `onAddLayersFinished()`: Procesa resultado de agregación
  - `onRemoveLayerFinished()`: Procesa resultado de eliminación

- **Actualizado:** Destructor para limpiar correctamente watchers en ejecución

### 3. `CMakeLists.txt`
- **Agregado:** `Concurrent` a la búsqueda de componentes Qt6
- **Agregado:** `Qt6::Concurrent` a las librerías enlazadas

## Flujo de ejecución

### Antes (sincrónico - bloquea UI)
```
QML ? createGeoPackage() ? GDAL ops (mucho tiempo) ? UI bloqueada ? regresa resultado
```

### Después (asincrónico - UI responsiva)
```
QML ? createGeoPackage() ? QtConcurrent::run(worker) ? regresa inmediatamente
                            ? (en hilo de trabajo)
                       GDAL ops (mucho tiempo)
                            ?
                    onCreateGeoPackageFinished()
                            ?
                    Actualiza UI (layerNames, busy, etc.)
```

## Cambios de comportamiento

### Para QML/Usuario Final
1. **Operaciones ahora son asincrónicas:**
   - Los métodos `createGeoPackage()`, `openGeoPackage()`, `addLayers()`, `removeLayer()` devuelven `true` inmediatamente
   - La propiedad `busy` cambia a `true` al inicio y vuelve a `false` cuando termina
   - Se emite `operationSucceeded()` o se establece `lastError` cuando termina

2. **UI no se congela:**
   - Carga SHP grandes sin bloquear la interfaz
   - El usuario puede interactuar con otros elementos mientras se procesa

3. **Progreso de operación:**
   - La propiedad `busy` indica que hay operación en curso
   - Los paneles QML ya vinculan a esta propiedad (ej: desabilitar botones, mostrar spinner)

### Para Desarrolladores
1. **Lógica GDAL aislada en workers:**
   - Métodos worker no acceden directamente a `this` (thread-safe)
   - Usan variables capturadas en lambda (copias)

2. **Resultados estructurados:**
   - Cada operación retorna una estructura con `success`, `errorMsg`, datos relevantes
   - Los handlers procesan resultados de forma thread-safe en el hilo UI

3. **Limpieza automática de recursos:**
   - Destructor limpia watchers pendientes
   - `GdalDatasetGuard` RAII asegura cierre de datasets en workers

## Impacto de rendimiento

### Antes
- SHP de 100 MB: ~5-10 segundos de congelación de UI
- Usuario experimenta app "no responde"

### Después
- Misma operación, pero UI permanece responsiva
- El trabajo ocurre en paralelo en pool de hilos
- Múltiples operaciones pueden ejecutarse simultáneamente (si se llaman antes de terminar)

## Consideraciones de thread-safety

1. **Variables capturadas por valor en lambdas:**
   ```cpp
   QtConcurrent::run([this, shpPaths, gpkgPath]() { ... })
   ```
   - `shpPaths`, `gpkgPath` son copias seguras
   - `this` es válido durante la ejecución (GeoManager vive en UI thread)

2. **GDAL es thread-safe (con restricciones):**
   - Cada worker tiene su propio `GdalDataset*`
   - No hay acceso concurrente al mismo dataset
   - `GDALAllRegister()` es seguro llamar una sola vez (ya hecho en constructor)

3. **Actualizaciones del modelo desde handlers (en UI thread):**
   - `setActiveGpkgPath()`, `setLastError()`, `setBusy()` emiten signals
   - QML se actualiza automáticamente

## Pruebas recomendadas

1. **Cargar SHP grande:**
   - Verificar que UI no se congela
   - Comprobar que propiedades `busy`, `layerNames` se actualizan correctamente

2. **Operaciones canceladas:**
   - Cerrando la app mientras hay operación en curso
   - Destructor debe limpiar correctamente

3. **Múltiples operaciones rápidas:**
   - Crear GPKG, luego abrir otra, luego agregar capas
   - Verificar que watchers se limpian y no hay conflictos

4. **Monitoreo de hilos:**
   - Con herramienta como Performance Profiler de Qt Creator
   - Verificar que operaciones se distribuyen entre hilos

## Requisitos de compilación

- Qt 6.5+ (incluye QtConcurrent)
- CMake 3.21+
- MSVC 2022

## Migración futura de otras operaciones

Las mismas técnicas pueden aplicarse a:
- `buildQgsXml()` en `QgisProjectGenerator` (genera XML/ZIP para `.qgz`)
- `getGpkgFileInfo()`, `getAllLayerInfo()` (si tienen lecturas grandes)

Patrón a seguir:
1. Crear estructura de resultado
2. Crear método worker que devuelva la estructura
3. Modificar método público para usar `QtConcurrent::run()` + `QFutureWatcher`
4. Agregar handler para procesar resultado en UI thread
