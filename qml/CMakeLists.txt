if(NOT TARGET TinyQtScrcpy)
  message(FATAL_ERROR "The TinyQtScrcpy core target must be defined before the QML module")
endif()

set(TINYQTSCRCPY_QML_URI Scrcpy)
string(REPLACE "." "/" TINYQTSCRCPY_QML_URI_PATH ${TINYQTSCRCPY_QML_URI})

qt_add_qml_module(TinyScrcpy
  URI ${TINYQTSCRCPY_QML_URI}
  VERSION ${PROJECT_VERSION}
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${TINYQTSCRCPY_QML_URI_PATH}"
  SOURCES
    ScrcpyView.hpp
    ScrcpyView.cpp
)

target_link_libraries(TinyScrcpy PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Qml
  Qt6::Quick
  Qt6::Network
  Qt6::Multimedia
  TinyQtScrcpy
  PkgConfig::FFMPEG
)

target_compile_definitions(TinyScrcpy PRIVATE
  SCRCPY_SERVER_DEFAULT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../core/src/third_party/scrcpy-server"
)

if(TINYQTSCRCPY_INSTALL)
  set(TINYQTSCRCPY_QML_INSTALL_DIR "${QML_INSTALL_DIR}/${TINYQTSCRCPY_QML_URI_PATH}")

  install(TARGETS TinyScrcpy TinyScrcpyplugin
    DESTINATION "${TINYQTSCRCPY_QML_INSTALL_DIR}"
  )

  qt_query_qml_module(TinyScrcpy QMLDIR _qmldir TYPEINFO _typeinfo)
  install(FILES ${_qmldir} ${_typeinfo}
    DESTINATION "${TINYQTSCRCPY_QML_INSTALL_DIR}"
  )
endif()
