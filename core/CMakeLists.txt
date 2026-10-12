if(TARGET TinyQtScrcpy)
  return()
endif()

set(TINYQTSCRCPY_CORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT EXISTS "${TINYQTSCRCPY_CORE_ROOT}/include/QtScrcpyCore.h")
  message(FATAL_ERROR "TinyQtScrcpy core sources missing at ${TINYQTSCRCPY_CORE_ROOT}")
endif()

set(TINYQTSCRCPY_CORE_SOURCES
  "${TINYQTSCRCPY_CORE_ROOT}/include/adbprocess.h"
  "${TINYQTSCRCPY_CORE_ROOT}/include/IDecoder.h"
  "${TINYQTSCRCPY_CORE_ROOT}/include/QtScrcpyCore.h"
  "${TINYQTSCRCPY_CORE_ROOT}/include/QtScrcpyCoreDef.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/adb/adbprocess.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/adb/adbprocessimpl.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/adb/adbprocessimpl.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/common/qscrcpyevent.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/compat.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/device.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/device.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/android/input.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/android/keycodes.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/bufferutil.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/bufferutil.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/controller.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/controller.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/inputconvert/controlmsg.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/inputconvert/controlmsg.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/inputconvert/inputconvertbase.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/inputconvert/inputconvertbase.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/inputconvert/inputconvertnormal.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/inputconvert/inputconvertnormal.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/receiver/devicemsg.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/receiver/devicemsg.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/receiver/receiver.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/receiver/receiver.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/avframeconvert.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/avframeconvert.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/decoder.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/decoder.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/fpscounter.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/fpscounter.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/videobuffer.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder/videobuffer.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/demuxer/demuxer.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/demuxer/demuxer.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/filehandler/filehandler.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/filehandler/filehandler.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/server/server.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/server/server.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/server/tcpserver.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/server/tcpserver.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/server/videosocket.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/server/videosocket.h"
  "${TINYQTSCRCPY_CORE_ROOT}/src/devicemanage/devicemanage.cpp"
  "${TINYQTSCRCPY_CORE_ROOT}/src/devicemanage/devicemanage.h"
)

qt_add_library(TinyQtScrcpy STATIC ${TINYQTSCRCPY_CORE_SOURCES})
add_library(TinyQtScrcpy::TinyQtScrcpy ALIAS TinyQtScrcpy)

set_target_properties(TinyQtScrcpy PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(TinyQtScrcpy PUBLIC
  "${TINYQTSCRCPY_CORE_ROOT}/include"
)
target_include_directories(TinyQtScrcpy PRIVATE
  "${TINYQTSCRCPY_CORE_ROOT}/src/adb"
  "${TINYQTSCRCPY_CORE_ROOT}/src/common"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/android"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/inputconvert"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/controller/receiver"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/decoder"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/demuxer"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/filehandler"
  "${TINYQTSCRCPY_CORE_ROOT}/src/device/server"
  "${TINYQTSCRCPY_CORE_ROOT}/src/devicemanage"
)

target_link_libraries(TinyQtScrcpy
  PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
  PRIVATE
    PkgConfig::FFMPEG
)
