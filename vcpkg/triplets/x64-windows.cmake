set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)

# Fuerza el uso del toolset v143 (VS2022). La instancia exacta de Visual
# Studio a usar se selecciona mediante la variable de entorno del SISTEMA
# VCPKG_VISUAL_STUDIO_PATH (no se puede fijar aqu\u00ed, vcpkg la lee del entorno
# del proceso, no del script de triplet).
set(VCPKG_PLATFORM_TOOLSET "v143")

