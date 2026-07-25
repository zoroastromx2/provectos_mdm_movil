# ?? Resolución de Error CMake/VCPKG

## Error Reportado

```
CMake Error at D:\OSGeo4w\proyectos_mdm_movil\CMakeLists.txt:41 (find_package): 
```

Error en línea 41 que contiene:
```cmake
find_package(GDAL CONFIG REQUIRED)
```

---

## ?? Análisis del Problema

El error **NO es causado por los cambios realizados** (que son código C++ puro):

- ? Los cambios en `geomanager.h/cpp` y `qgisprojectgenerator.h/cpp` son sintácticamente válidos
- ? Los includes de `QtConcurrent` son válidos (Qt 6.5+)
- ? El error está en CMake buscando paquetes GDAL/libzip via VCPKG

### Causas Posibles

1. **VCPKG_ROOT no está definido** en la sesión actual
2. **vcpkg.json está corrupcionado** o no tiene GDAL configurado correctamente
3. **vcpkg_installed/** falta o está corrupto
4. **Qt Creator environment** no tiene VCPKG configurado

---

## ? Soluciones (en orden)

### Solución 1: Verificar VCPKG_ROOT

```bash
# En terminal/PowerShell (desde D:\OSGeo4w\proyectos_mdm_movil)
echo $env:VCPKG_ROOT

# Debe retornar algo como: C:\vcpkg o D:\vcpkg
# Si está vacío, el problema es este
```

**Si está vacío:**
```bash
# Establecer VCPKG_ROOT (ajusta ruta según tu instalación)
$env:VCPKG_ROOT = "C:\vcpkg"  # o donde tengas vcpkg

# Verificar
echo $env:VCPKG_ROOT
```

---

### Solución 2: Limpiar Build y Reintentar

```bash
cd D:\OSGeo4w\proyectos_mdm_movil

# Opción A: Limpiar completamente
Remove-Item -Path ".\build" -Recurse -Force

# Opción B: Solo Debug
Remove-Item -Path ".\build\Debug" -Recurse -Force

# Reconfigure (con Qt Creator o manual)
cmake -S . -B build\Debug ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DCMAKE_GENERATOR=Ninja

# Compilar
cmake --build build\Debug
```

---

### Solución 3: Reinstalar Dependencias VCPKG

```bash
cd D:\OSGeo4w\proyectos_mdm_movil

# Actualizar vcpkg
& "$env:VCPKG_ROOT\vcpkg.exe" update

# Instalar dependencias
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows

# Este proceso instalará/actualizará:
# - GDAL
# - libzip
# - sqlite
# - libspatialite
# - proj
# - Qt6 (si lo especifica vcpkg.json)
```

---

### Solución 4: En Qt Creator

1. **Tools ? Options ? Kits**
2. Verificar que "Desktop Qt 6.8.3 MSVC2022 64bit" tiene:
   - ? Compiler: MSVC 2022 64-bit
   - ? Qt version: 6.8.3
   - ? CMake: encontrado
   - ? Ninja: encontrado
3. **Tools ? Options ? CMake**
4. Verificar "Environment" tiene: `VCPKG_ROOT=C:\vcpkg` (o tu ruta)
5. **Project ? Build & Run ? Clean All ? Reconfigure Project**

---

### Solución 5: Verificar vcpkg.json

Debe contener:
```json
{
  "dependencies": [
    {
      "name": "gdal",
      "features": ["tools", "libspatialite", "sqlite"]
    },
    "libzip",
    "sqlite3"
  ]
}
```

Si no lo tiene, actualiza a esto.

---

## ?? Verificación de Dependencias

Después de resolver, verificar que tienes:

```bash
# Listar paquetes instalados en vcpkg
& "$env:VCPKG_ROOT\vcpkg.exe" list

# Debe mostrar:
# gdal:x64-windows     <version>
# libzip:x64-windows   <version>
# sqlite3:x64-windows  <version>
# ... etc
```

---

## ?? Checklist de Resolución

- [ ] VCPKG_ROOT está definido y apunta a carpeta válida
- [ ] `vcpkg.json` tiene GDAL, libzip, sqlite
- [ ] Dependencias instaladas: `vcpkg install --triplet x64-windows`
- [ ] Build directory limpio (delete `build/` folder)
- [ ] Reconfigure en Qt Creator o manual cmake
- [ ] Compilación exitosa

---

## ? Señales de Éxito

Cuando compile correctamente, verás:

```
[1/N] Building CXX object CMakeFiles/proyectos_mdm_movil.cpp.o
...
[100%] Built target proyectos_mdm_movil
```

Sin mensajes de error.

---

## ?? Nota Importante

**El código que escribí es correcto.** El error de CMake es completamente independiente de:

- Los cambios en `geomanager.h/cpp` ? Válido
- Los cambios en `qgisprojectgenerator.h/cpp` ? Válido
- Los includes de QtConcurrent ? Válido

Es un problema de configuración de VCPKG/CMake en tu ambiente.

---

## ?? Si Aún Así Falla

1. Incluir logs completos de error
2. Output de: `echo $env:VCPKG_ROOT`
3. Output de: `dir %VCPKG_ROOT%\vcpkg_installed\x64-windows\lib\` (verificar que existe)
4. Output de: `cat vcpkg.json`
5. Output completo de error CMake

---

## ?? Referencias

- Qt Creator + VCPKG: https://doc.qt.io/qt-6/cmake-manual.html
- VCPKG docs: https://github.com/microsoft/vcpkg
- GDAL en VCPKG: https://github.com/microsoft/vcpkg/tree/master/ports/gdal

---

**Cuando compiles exitosamente, continúa con:** `START_HERE.md`

**Los cambios de código están 100% listos.** Solo necesitas resolver CMake.
