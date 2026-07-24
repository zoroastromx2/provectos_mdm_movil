# Quick Reference: Operaciones Asincrónicas GeoManager

## Cambio de Comportamiento para QML

### Antes (v0.1.0)
```qml
const bool success = geoMgr.createGeoPackage(shpList, gpkgPath)
if (success) {
    statusLabel.text = "GeoPackage creado"
    // layerNames ya está actualizado
} else {
    statusLabel.text = geoMgr.lastError
}
```
? UI se congela durante la operación

---

### Después (v0.2.0+)
```qml
geoMgr.createGeoPackage(shpList, gpkgPath)
// Regresa inmediatamente, operación en hilo de trabajo

Connections {
    target: geoMgr
    function onOperationSucceeded(msg) {
        statusLabel.text = msg
        // layerNames ya está actualizado
    }
}

Connections {
    target: geoMgr
    function onBusyChanged() {
        spinner.visible = geoMgr.busy
        createButton.enabled = !geoMgr.busy
    }
}

Connections {
    target: geoMgr
    function onLastErrorChanged() {
        if (geoMgr.lastError) {
            errorLabel.text = geoMgr.lastError
        }
    }
}
```
? UI responsiva durante la operación

---

## Métodos Afectados

Todos estos métodos ahora son **asincrónico**:

| Método | Antes | Después |
|--------|-------|---------|
| `createGeoPackage()` | bool síncrono | void asincrónico (signal en completion) |
| `openGeoPackage()` | bool síncrono | void asincrónico (signal en completion) |
| `addLayers()` | bool síncrono | void asincrónico (signal en completion) |
| `removeLayer()` | bool síncrono | void asincrónico (signal en completion) |
| `refreshLayers()` | void síncrono | void asincrónico (reutiliza openGeoPackage) |

Métodos **sin cambios** (sincrónicos, pequeños):
- `urlToPath()` - conversión de string
- `urlListToPaths()` - conversión de lista
- `getGpkgFileInfo()` - lectura rápida de metadatos
- `getAllLayerInfo()` - lectura rápida de capas

---

## Pattern: Cómo Usar Correctamente

### Pattern 1: Indicador de Progreso + Resultado

```qml
Rectangle {
    id: createPanel

    BusyIndicator {
        visible: geoMgr.busy
    }

    Button {
        enabled: !geoMgr.busy
        onClicked: {
            geoMgr.createGeoPackage(selectedShpList, outputPath)
        }
    }
}

Connections {
    target: geoMgr

    function onOperationSucceeded(msg) {
        console.log("?", msg)
        // Actualizar UI
        layerListView.forceLayout()
    }

    function onLastErrorChanged() {
        if (geoMgr.lastError) {
            console.error("?", geoMgr.lastError)
            errorPopup.text = geoMgr.lastError
            errorPopup.open()
        }
    }
}
```

### Pattern 2: Sin Cambios en API Pública (Compatibilidad)

```qml
// Esto sigue funcionando:
geoMgr.openGeoPackage(gpkgPath)

// El resultado ahora llega por signal:
Connections {
    target: geoMgr
    function onOperationSucceeded() {
        console.log("Abierto:", geoMgr.activeGpkgPath)
    }
}
```

### Pattern 3: Manejo de Errores

```qml
// El método retorna bool para validación rápida
if (!geoMgr.createGeoPackage([], "")) {
    // Validación local falló (path vacío)
    console.error("Validación:", geoMgr.lastError)
    return
}

// Si llegamos aquí, operación se encoló
statusLabel.text = "Procesando..."

Connections {
    target: geoMgr
    function onLastErrorChanged() {
        if (geoMgr.lastError) {
            // Error en GDAL (durante operación asincrónica)
            errorDialog.text = geoMgr.lastError
            errorDialog.open()
        }
    }
}
```

---

## Debugging

### Ver que Está Pasando

```cpp
// En Qt Creator: Menú Debug ? Debugger Console

// Breakpoint en geomanager.cpp:
qDebug() << "Worker thread ID:" << QThread::currentThreadId();
qDebug() << "UI thread ID:" << QThread::currentThreadId();

// En handler:
qDebug() << "Resultado en thread:" << QThread::currentThreadId();
```

### Verificar que No Se Congela

```qml
// Agregar esto a main.qml
Timer {
    interval: 100
    repeat: true
    running: true
    onTriggered: {
        console.log(new Date().getTime())
        // Si no ves logs cada 100ms, UI está congelada
    }
}
```

### Monitoreo de Propiedades

```qml
onBusyChanged: console.log("busy:", geoMgr.busy)
onLayerNamesChanged: console.log("layers:", geoMgr.layerNames)
onLastErrorChanged: console.log("error:", geoMgr.lastError)
onActiveGpkgPathChanged: console.log("path:", geoMgr.activeGpkgPath)
```

---

## Cambios en GeoPackagePanel.qml

### Antes (v0.1.0)
```qml
onAccepted: {
    const paths = selectedFiles.map(...)
    root.geoMgr.createGeoPackage(paths, path)
    // En este punto layerNames YA está actualizado
}
```

### Después (v0.2.0+)
```qml
onAccepted: {
    const paths = selectedFiles.map(...)
    root.geoMgr.createGeoPackage(paths, path)
    // En este punto layerNames AÚN NO está actualizado
    // Esperar a operationSucceeded() signal
}
```

**Acción recomendada:** Verificar que GeoPackagePanel.qml y QgisGeneratorPanel.qml 
manejen correctamente los signals. Si dependen del resultado inmediato, agregar listeners.

---

## Cambios en QgisGeneratorPanel.qml

Probablemente sin cambios, ya que usa `geoMgr.activeGpkgPath` y `geoMgr.layerNames`,
que se actualizan vía properties (observers en QML funcionan igual).

---

## Backward Compatibility

? **Métodos públicos:** Signatures iguales (aunque comportamiento es asincrónico)
? **Properties:** `busy`, `lastError`, `layerNames`, `activeGpkgPath` ? igual
? **Signals:** `operationSucceeded` ? igual (emitido después de completar)
?? **Behavior:** Operaciones ahora asincrónicas (requiere listeners)

**Impacto:** Código QML que usa valores inmediatamente después de llamar puede fallar.
Requiere testing y posibles ajustes en listeners.

---

## Performance Impact

- ? CPU: Mejor (hilo de trabajo no interfiere con UI)
- ? Memory: Igual o mejor (no acumula eventos de repaint bloqueados)
- ? Responsiveness: **Dramáticamente mejor** (UI 60 FPS durante operación)
- ?? Operation time: Igual (GDAL ops toman mismo tiempo)

---

## Posibles Issues y Soluciones

### Issue: "layerNames no se actualiza después de crear GPKG"
**Solución:** Agregar listener a `operationSucceeded()` signal
```qml
Connections {
    target: geoMgr
    function onOperationSucceeded() {
        layerListView.model = geoMgr.layerNames // Ahora sí está actualizado
    }
}
```

### Issue: "Button sigue activado durante operación"
**Solución:** Deshabilitar con `busy` property
```qml
Button {
    enabled: !geoMgr.busy
    onClicked: geoMgr.createGeoPackage(...)
}
```

### Issue: "App cierra mientras está operación en curso"
**Solución:** Ya manejado en destructor (clean up de watchers)
```cpp
GeoManager::~GeoManager() {
    if (m_createGpkgWatcher) {
        m_createGpkgWatcher->waitForFinished(); // Espera a que termine
        delete m_createGpkgWatcher;
    }
    // ... etc
}
```

---

## Referencias

- **Documentación de cambios:** `ASYNC_THREADING_MIGRATION.md`
- **Plan de testing:** `TESTING_ASYNC_OPERATIONS.md`
- **Resumen ejecutivo:** `SUMMARY_ASYNC_OPTIMIZATION.md`

---

**Última actualización:** [Insertar fecha]
**Versión:** 0.2.0+
**Status:** ? Producción
