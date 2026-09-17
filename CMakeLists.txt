cmake_minimum_required(VERSION 3.19)
project(EquicordPluginManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
  if(EXISTS "C:/Qt/6.8.3/msvc2022_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")
  endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CORE_SOURCES
  src/core/archive.cpp
  src/core/builder.cpp
  src/core/equicord.cpp
  src/core/paths.cpp
  src/core/persist.cpp
  src/core/plugin.cpp
  src/core/runner.cpp
  src/net/http.cpp
  src/ui/log.cpp
)

set(QT_SOURCES
  src/qt/main.cpp
  src/qt/mainwindow.cpp
  src/qt/mainwindow.h
  src/qt/pluginwidget.cpp
  src/qt/pluginwidget.h
  src/qt/settingsdialog.cpp
  src/qt/settingsdialog.h
  src/qt/settingsdialog.ui
  src/qt/aboutdialog.cpp
  src/qt/aboutdialog.h
  src/qt/aboutdialog.ui
  qt.qrc
)

add_executable(EquicordPluginManagerQt WIN32
  ${CORE_SOURCES}
  ${QT_SOURCES}
  resource.rc
)

target_include_directories(EquicordPluginManagerQt PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src/qt
)

target_compile_definitions(EquicordPluginManagerQt PRIVATE UNICODE _UNICODE NOMINMAX)

target_link_libraries(EquicordPluginManagerQt PRIVATE
  Qt6::Widgets
  user32 gdi32 comctl32 ole32 uuid shell32 shlwapi dwmapi winhttp advapi32 comdlg32 gdiplus uxtheme
)
target_link_options(EquicordPluginManagerQt PRIVATE "/MANIFEST:NO")
set_source_files_properties(resource.rc PROPERTIES COMPILE_FLAGS "/nologo")

set_target_properties(EquicordPluginManagerQt PROPERTIES
  OUTPUT_NAME "PluginManager"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/build"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/build"
)

find_program(WINDEPLOYQT_EXE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
if(WINDEPLOYQT_EXE)
  add_custom_command(TARGET EquicordPluginManagerQt POST_BUILD
    COMMAND "${WINDEPLOYQT_EXE}" --no-translations --no-system-d3d-compiler --no-opengl-sw "$<TARGET_FILE:EquicordPluginManagerQt>"
    COMMENT "Running windeployqt..."
  )
endif()
