# Guía de Testing: Operaciones Asincrónicas GDAL

## Checklist de Validación

### 1. UI No se Congela (Funcionalidad Principal)

#### Test 1.1: Cargar SHP Grande
**Pasos:**
1. Abrir la aplicación
2. Ir al panel "GeoPackage"
3. Hacer clic en "Crear nuevo GeoPackage"
4. Seleccionar un SHP grande (>50 MB)
5. Especificar ruta de salida y crear

**Resultado esperado:**
- La interfaz permanece responsiva (botones son clickeables, scroll funciona)
- La propiedad `busy` muestra que hay operación en curso
- Un spinner o indicador visual muestra progreso
- Después de unos segundos, aparece "GeoPackage creado"

**Verificación:**
- ? UI no se congela
- ? `busy` cambia a true ? false
- ? Mensaje de éxito aparece
- ? Archivo GPKG se crea correctamente

---

#### Test 1.2: Agregar Capas SHP Grandes
**Pasos:**
1. Abrir GeoPackage existente
2. Hacer clic en "Agregar Shapefiles"
3. Seleccionar múltiples SHP grandes
4. Ejecutar agregación

**Resultado esperado:**
- UI permanece responsiva durante toda la operación
- Capas aparecen en la lista cuando termina
- Mensaje "Capas agregadas exitósamente"

---

### 2. Manejo de Errores

#### Test 2.1: Archivo SHP Corrupto
**Pasos:**
1. Intentar crear GPKG con SHP corrupto
2. Observar comportamiento

**Resultado esperado:**
- Operación continúa con otros archivos (no fatal)
- Se establece `lastError` con descripción del problema
- `busy` vuelve a false
- Otros SHP válidos se importan

---

#### Test 2.2: Ruta Inválida
**Pasos:**
1. Intentar guardar GPKG en ruta no accesible (ej: `C:\sys:\invalid`)
2. Observar resultado

**Resultado esperado:**
- `lastError` muestra error descriptivo
- `busy` vuelve a false
- UI sigue responsiva

---

### 3. Integridad de Datos

#### Test 3.1: GPKG Creado es Válido
**Pasos:**
1. Crear GPKG con varios SHP
2. Abrir con QGIS 3.40
3. Verificar capas, geometrías, atributos

**Resultado esperado:**
- QGIS abre el GPKG sin errores
- Todas las capas visibles
- Datos y geometrías correctas
- CRS identificado correctamente

---

#### Test 3.2: Capas Agregadas son Válidas
**Pasos:**
1. Crear GPKG simple
2. Abrir en aplicación
3. Agregar más SHP
4. Abrir con QGIS

**Resultado esperado:**
- Las nuevas capas aparecen en QGIS
- Datos originales intactos
- No hay duplicados

---

### 4. Señales y Properties

#### Test 4.1: `layerNamesChanged` se Emite
**Pasos:**
1. Observar la propiedad `layerNames` en QML
2. Crear GPKG o agregar capas
3. Verificar que se actualiza

**Resultado esperado:**
- `layerNames` lista se actualiza después de completar operación
- Componentes QML vinculados se refrescan

---

#### Test 4.2: `operationSucceeded` se Emite
**Pasos:**
1. Monitorear signal `operationSucceeded` en QML
2. Completar una operación exitosa
3. Verificar que se emite el mensaje correcto

**Resultado esperado:**
- Signal se emite con mensaje descriptivo
- Mensaje aparece en UI (si está conectado)

---

#### Test 4.3: `lastError` se Establece en Fallo
**Pasos:**
1. Intentar operación que falla
2. Observar `lastError`

**Resultado esperado:**
- `lastError` contiene descripción del error
- Se limpia en operación exitosa siguiente

---

### 5. Limpieza de Recursos

#### Test 5.1: Cierre Durante Operación
**Pasos:**
1. Iniciar operación larga (crear GPKG grande)
2. Mientras está en progreso, cerrar la aplicación
3. Observar en debugger que destructores se llaman

**Resultado esperado:**
- No hay memory leaks
- Watchers se limpian correctamente
- App termina sin crashes

---

#### Test 5.2: Múltiples Operaciones Consecutivas
**Pasos:**
1. Crear GPKG #1
2. Inmediatamente después, crear GPKG #2
3. Luego agregar capas
4. Repetir 5 veces

**Resultado esperado:**
- No hay interferencia entre operaciones
- Cada resultado es correcto
- No hay memory leaks
- `busy` siempre es consistente

---

### 6. Performance Baseline

#### Test 6.1: Medir Tiempo de Operación
**Pasos:**
1. Cronometrar creación de GPKG con SHP de 10 MB
2. Anotar tiempo total
3. Repetir con SHP de 50 MB, 100 MB

**Resultado esperado:**
- 10 MB: ~1-2 segundos
- 50 MB: ~5-10 segundos  
- 100 MB: ~15-30 segundos
- (Valores aproximados, dependen de CPU/disco)

**Nota:** El tiempo es similar al antes (operación GDAL no cambia), pero ahora UI es responsiva

---

#### Test 6.2: Comparativa: Antes vs Después
**Medición: Tiempo de UI Response Durante Operación**

Si tienes versión anterior:
- **Antes:** UI congela ~100% del tiempo de operación
- **Después:** UI responde en <50ms incluso durante operación

---

### 7. Casos Extremos

#### Test 7.1: Operación Cancelada por Usuario
**Pasos:**
1. Iniciar operación larga
2. En medio, hacer clic en botón para salir del panel/ventana
3. Observar estado

**Resultado esperado:**
- Operación en hilo continúa en background (no falla)
- UI sigue responsiva
- Si vuelves al panel, estado es consistente

---

#### Test 7.2: Archivo GPKG Usado por Otro Programa
**Pasos:**
1. Abrir GPKG en QGIS
2. Intentar agregar capas desde aplicación
3. Observar resultado

**Resultado esperado:**
- Operación falla con `lastError` descriptivo (lockfile, permisos)
- UI no se congela
- Ambas aplicaciones siguen funcionando

---

## Herramientas de Debugging

### Profiler de Qt Creator
1. Abrir proyecto en Qt Creator
2. Menú: Analyze ? QML Profiler
3. Ejecutar aplicación
4. Registrar operación
5. Observar en Timeline:
   - Eventos del hilo UI (no deben bloquearse)
   - Hilos de trabajo (deben tener picos de CPU)

### Inspección de Thread
1. Breakpoint en `onCreateGeoPackageFinished()`
2. Verificar que `QThread::currentThread()` es hilo UI
3. Verificar que worker se ejecutó en otro hilo

### Logging Manual
Agregar en `geomanager.cpp`:
```cpp
qDebug() << "Worker comenzó en thread:" << QThread::currentThreadId();
qDebug() << "Handler ejecutándose en thread:" << QThread::currentThreadId();
```

---

## Reporte de Issues

Si encuentras problema:

1. **Describe la operación:** (crear GPKG, agregar capas, etc.)
2. **Tamaño de archivos:** (SHP de X MB)
3. **Comportamiento observado:** (UI congela, crash, etc.)
4. **Logs:**
   ```cpp
   // Verificar en Application Output de Qt Creator
   // Buscar qWarning(), qCritical()
   ```
5. **Reproducibiidad:** (siempre pasa, a veces, con ciertos archivos)

---

## Versión de Baseline

Cambios introducidos en: Migración QtConcurrent (rama: async-threading)

**Fecha:** [Insertar fecha de commit]
**Archivos modificados:**
- `src/geomanager.h`
- `src/geomanager.cpp`
- `CMakeLists.txt`

**Compatibilidad QML:** Sin cambios (API pública igual)
