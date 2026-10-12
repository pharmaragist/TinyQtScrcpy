# TinyQtScrcpy

A trimmed, Linux-only fork of [QtScrcpyCore](https://github.com/barry-ran/QtScrcpyCore)
that exposes Android screen mirroring and control as a reusable Qt 6 / QML module.

It bundles two pieces:

- `core/` — the de-vendored scrcpy client library (`TinyQtScrcpy` C++ static
  library, based on QtScrcpyCore).
- `qml/` — the `Scrcpy` QML module exposing the `ScrcpyView` element, which
  renders the device video stream into any `QVideoSink` and forwards pointer,
  wheel, keyboard, navigation and file-transfer input.

The bundled `scrcpy-server` (v4.1) is pushed to the device at runtime; `adb` is
resolved from `PATH` (or a path set through `ScrcpyView.adbPath`).

## Build

Requires Qt 6.5+ and system FFmpeg (libavformat, libavcodec, libavutil,
libswscale) discoverable through `pkg-config`.

```sh
cmake --preset default
cmake --build --preset default
sudo cmake --install build
```

The QML module is installed to `<Qt qml dir>/Scrcpy`, so consumers use it with
`import Scrcpy`.

## Use

```qml
import QtQuick
import QtMultimedia
import Scrcpy

VideoOutput {
    id: view
    ScrcpyView {
        id: scrcpy
        videoSink: view.videoSink
        serial: "YOUR_DEVICE_SERIAL"
        Component.onCompleted: start(serial)
    }
}
```

## Embedding in another CMake project

```cmake
include(FetchContent)
FetchContent_Declare(TinyQtScrcpy
  GIT_REPOSITORY git@github.com:pharmaragist/TinyQtScrcpy.git
  GIT_TAG main
)
FetchContent_MakeAvailable(TinyQtScrcpy)
```

To build against a local checkout instead of the remote, point CMake at it:

```sh
cmake -DFETCHCONTENT_SOURCE_DIR_TINYQTSCRCPY=/path/to/TinyQtScrcpy ..
```

## Changes vs upstream

- Linux only. Removed macOS VideoToolbox, Windows/macOS code paths, and the
  bundled FFmpeg and adb payloads.
- Removed game key-mapping input.
- Qt6-only input conversion (no Qt5 `QRegExp` / `Qt::MidButton` branches).
- FFmpeg 8 compatibility (no `avcodec_close`) with VA-API hardware decode and
  automatic software fallback (opt out with `NOON_SCRCPY_SOFTWARE_DECODE=1`).
- Recorder removed; screenshot and file handling retained.

## License

Apache-2.0. See `LICENSE`. Upstream: QtScrcpyCore by barry-ran, pinned at
commit `9e388b8aa48e4e1c2cfdef408ef00c4c6b45c921`.
