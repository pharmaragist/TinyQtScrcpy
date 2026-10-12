cmake_minimum_required(VERSION 3.21)
project(TinyQtScrcpy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

set(TINYQTSCRCPY_QT_MIN_VERSION 6.5)

find_package(Qt6 ${TINYQTSCRCPY_QT_MIN_VERSION} REQUIRED COMPONENTS
  Core
  Gui
  Network
  Qml
  Quick
  Multimedia
)
qt_standard_project_setup(REQUIRES ${TINYQTSCRCPY_QT_MIN_VERSION})

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET GLOBAL
  libavformat
  libavcodec
  libavutil
  libswscale
)

include(GNUInstallDirs)

if(NOT QML_INSTALL_DIR)
  set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
      CACHE PATH "QML module install dir")
endif()

option(TINYQTSCRCPY_INSTALL "Install the QML module" ON)

add_subdirectory(core)
add_subdirectory(qml)
