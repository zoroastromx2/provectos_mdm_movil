set(SCRIPT_PATH "${CURRENT_INSTALLED_DIR}/share/qtbase")
include("${SCRIPT_PATH}/qt_install_submodule.cmake")

vcpkg_buildpath_length_warning(44)

set(${PORT}_PATCHES "")

# Known Qt-on-Windows bug: moc.exe crashes with 0xC0000005 (access violation)
# when many AutoMoc subprocesses run concurrently (parallel build file-access
# conflicts). Quick_autogen runs moc for dozens of headers at once. Reduce the
# parallelism just for this package to keep the build reliable.
set(VCPKG_CONCURRENCY 4)

# Ensure Qt host tool DLLs (moc, rcc, etc.) can be executed during build.
# moc.exe depends on pcre2-16.dll and other runtime DLLs that may not be on PATH
# inside vcpkg's sandboxed build environment. We copy them from the installed bin
# directory to wherever qtbase placed its host tools build tree.
if (VCPKG_TARGET_IS_WINDOWS)
  set(_qt_host_tools_dir "${CURRENT_HOST_INSTALLED_DIR}/tools/Qt6/bin")
  set(_qt_runtime_dlls
    double-conversion.dll
    icudt74.dll
    icuin74.dll
    icuuc74.dll
    pcre2-16.dll
    zlib1.dll
    zstd.dll
  )
  foreach(_dll IN LISTS _qt_runtime_dlls)
    set(_src "${CURRENT_INSTALLED_DIR}/bin/${_dll}")
    set(_dst "${_qt_host_tools_dir}/${_dll}")
    if (EXISTS "${_src}" AND NOT EXISTS "${_dst}")
      file(COPY "${_src}" DESTINATION "${_qt_host_tools_dir}")
    endif()
  endforeach()
  unset(_qt_host_tools_dir)
  unset(_qt_runtime_dlls)
  unset(_src)
  unset(_dst)
  unset(_dll)
endif()

set(TOOL_NAMES
    qml
    qmlaotstats
    qmlcachegen
    qmleasing
    qmlformat
    qmlimportscanner
    qmllint
    qmlplugindump
    qmlpreview
    qmlprofiler
    qmlscene
    qmltestrunner
    qmltime
    qmltyperegistrar
    qmldom
    qmltc
    qmlls
    qmljsrootgen
    svgtoqml
)

qt_install_submodule(
  PATCHES
  ${${PORT}_PATCHES}
  TOOL_NAMES
  ${TOOL_NAMES}
  CONFIGURE_OPTIONS
  -DFEATURE_quickcontrols2_fusion:BOOL=ON
  -DFEATURE_quickcontrols2_material:BOOL=ON
  -DFEATURE_quickcontrols2_imagine:BOOL=OFF
  -DFEATURE_quickcontrols2_universal:BOOL=OFF
  -DFEATURE_quickcontrols2_fluentwinui3:BOOL=OFF
  -DFEATURE_quickcontrols2_macos:BOOL=OFF
  -DFEATURE_quickcontrols2_ios:BOOL=OFF
  -DFEATURE_quickcontrols2_windows:BOOL=OFF
  -DCMAKE_DISABLE_FIND_PACKAGE_LTTngUST:BOOL=ON
  CONFIGURE_OPTIONS_RELEASE
  CONFIGURE_OPTIONS_DEBUG
)
