cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------
# vcpkg integration: point CMake at the vcpkg toolchain BEFORE project().
# Users can override by setting -DCMAKE_TOOLCHAIN_FILE=... on the command line
# or by setting the VCPKG_ROOT environment variable in Qt Creator.
# ---------------------------------------------------------------------------
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(proyectos_mdm_movil
    VERSION 0.1.0
    DESCRIPTION "GIS tool for GeoPackage management and QGIS project generation"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Qt auto-tools
# ---------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------------------
# Qt6 – require Core, Quick, Qml, Concurrent (and Widgets only for native dialogs helper)
# ---------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 Concurrent Widgets Xml Sql Svg Multimedia MultimediaWidgets UiTools Core5Compat QuickWidgets)

# ---------------------------------------------------------------------------
# GDAL (installed via vcpkg)
# ---------------------------------------------------------------------------
find_package(GDAL CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# libzip (installed via vcpkg)
# ---------------------------------------------------------------------------
find_package(libzip CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# QGIS (debe estar instalado en el sistema o via vcpkg/custom path)
# Ajusta QGIS_PREFIX_PATH si QGIS no está en las rutas estándar
# ---------------------------------------------------------------------------
# NOTA: Si QGIS no está en vcpkg, debes instalarlo manualmente y configurar:
# set(QGIS_PREFIX_PATH "C:/OSGeo4W64" CACHE PATH "QGIS installation path")
# list(APPEND CMAKE_PREFIX_PATH "${QGIS_PREFIX_PATH}")

#find_package(QGIS REQUIRED)
#if(NOT QGIS_FOUND)
#    message(FATAL_ERROR "QGIS no encontrado. Por favor instala QGIS e inicia QGIS_PREFIX_PATH.")
#endif()

# ---------------------------------------------------------------------------
# Executable + QML module
# ---------------------------------------------------------------------------
qt_add_executable(proyectos_mdm_movil
    src/main.cpp
    src/geomanager.cpp
    src/geomanager.h
    src/qgisprojectgenerator.cpp
    src/qgisprojectgenerator.h
    src/qgisprojectviewer.cpp
    src/qgisprojectviewer.h
)

qt_add_qml_module(proyectos_mdm_movil
    URI App
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/GeoPackagePanel.qml
        qml/GpkgViewerDialog.qml
        qml/QgisGeneratorPanel.qml
)

# ---------------------------------------------------------------------------
# Include paths for our source + QGIS
# ---------------------------------------------------------------------------
#target_include_directories(proyectos_mdm_movil PRIVATE
#    src
#    ${QGIS_INCLUDE_DIR}
#)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(proyectos_mdm_movil PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Widgets
    Qt6::Xml
    Qt6::Sql
    Qt6::Svg
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::UiTools
    Qt6::Core5Compat
    Qt6::QuickWidgets
    GDAL::GDAL
    libzip::zip
    #QGIS::Core
    #QGIS::Gui

)

# ---------------------------------------------------------------------------
# Windows: copy runtime DLLs to the output directory automatically.
# Qt deployment is handled separately via windeployqt.
# ---------------------------------------------------------------------------
if(WIN32)
    # Copy all DLLs that the linker needs at runtime (vcpkg-provided libraries)
    add_custom_command(TARGET proyectos_mdm_movil POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:proyectos_mdm_movil>
                $<TARGET_FILE_DIR:proyectos_mdm_movil>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs to output directory"
    )

    # Run windeployqt to copy the Qt Quick and QML runtime files
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET proyectos_mdm_movil POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    --no-translations
                    "$<TARGET_FILE:proyectos_mdm_movil>"
            COMMENT "Running windeployqt"
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Install rules (optional – useful for packaging)
# ---------------------------------------------------------------------------
install(TARGETS proyectos_mdm_movil
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
