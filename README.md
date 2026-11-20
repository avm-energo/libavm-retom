# libavm-retom

[![pipeline status](https://git.avmenergo.ru/avm-energo/libavm-retom/badges/master/pipeline.svg)](https://git.avmenergo.ru/avm-energo/libavm-retom/-/pipelines)
[![latest release](https://git.avmenergo.ru/avm-energo/libavm-retom/-/badges/release.svg)](https://git.avmenergo.ru/avm-energo/libavm-retom/-/releases)

Русская версия: [README-ru.md](README-ru.md)

**libavm-retom** is a driver library for **РЕТОМ-51** (RETOM-51) relay test-bench devices made by NPP Dinamika (<https://dynamics.com.ru/>), for AVM-Energo projects. Its GUI client is [avm-retom](https://git.avmenergo.ru/avm-energo/avm-retom).

This software was made by AVM-Energo LLC with the help of Claude.

The library talks to the device over its FTDI virtual COM port (`QSerialPort`,
VID/PID `0403:6001`).

The API surface implements exactly the subset so far: `connect`/`disconnect`, `enable`/`disable`,
`apply`/`off`/`safetyStop`, `readInputContacts`, `setOutContact`.

# Requires

- Tools:
  - C++ compiler with full C++17 support (MSVC 2019 16.10+, gcc 9.3.0+, clang-11+)
  - CMake 3.18+
- Dependencies:
  - Qt 6 (Core + SerialPort) - https://www.qt.io/. `find_package(QT NAMES Qt6 Qt5)`
    falls back to Qt 5 if 6 isn't found, but only Qt 6 has been tested.

# Build

```bash
$ cmake -B build -S . -DCMAKE_PREFIX_PATH=path_to_qt
$ cmake --build build --config Release
```

supported options:
- cmake
  - `CMAKE_PREFIX_PATH` - path to your Qt install, e.g. `C:\Qt\6.10.1\msvc2022_64`
  - `BUILD_SHARED_LIBS:BOOL=ON/OFF` - shared (`.dll`/`.so`) or static
    (`.lib`/`.a`) library. CMake's own default (`OFF`, i.e. static)
    applies unless you pass `ON`.
  - `CMAKE_BUILD_TYPE` - `Debug`/`Release`/`MinSizeRel`/`RelWithDebInfo`
    (defaults to `Release` if unset)
  - `RETOM_BUILD_TESTS:BOOL=ON/OFF` - build the QtTest-based offline unit
    tests in `tests/` (default `ON`)

`tests/` (offline QtTest unit tests, run via `ctest`) and `tools/` (a
manual live-hardware smoke tool) are only built when this is the
top-level CMake project - a consumer pulling this library in via
`add_subdirectory()`/`FetchContent` won't get them.

# Usage

```cpp
#include <libavm-retom/driver.h>

retom::RetomConfig cfg;
retom::RetomDriver driver(cfg);  // real FtdiTransport, found by VID/PID

driver.connect(/* enable = */ true);

retom::RetomSignalSet sigSet;
sigSet.u["A"] = 1.0;   // Ua = 1.0 V
sigSet.freq = 50.0;
driver.apply(sigSet);

driver.off();
driver.disconnect();
```

If the test bench has more than one FTDI device attached, pass an
explicit `FtdiTransport` with a COM port instead of relying on VID/PID
lookup:

```cpp
retom::RetomDriver driver(cfg, std::make_unique<retom::FtdiTransport>(
    retom::kRetomVid, retom::kRetomPid, "COM10"));
```

Every call throws `retom::RetomError` (or one of its subclasses -
`RetomNotConnected`, `RetomNotEnabled`, `RetomProtocolError`) on failure.
See `tools/live_smoke_test.cpp` for a complete, runnable program that
exercises the full connect/apply/off/setOutContact/disconnect lifecycle
against real hardware.

# Packaging

Linux packages are built by CI (`.gitlab-ci.yml`) with CPack: `libavm-retom` (runtime, the shared library) and
`libavm-retom-dev` (`.deb`) / `libavm-retom-devel` (`.rpm`) (static library, headers, CMake package files). To build them
locally:

```bash
$ cmake -B build -S . -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr
$ cmake --build build
$ cd build && cpack -G DEB   # or -G RPM
```

Packages of every tagged version are listed on the [Releases page](https://git.avmenergo.ru/avm-energo/libavm-retom/-/releases).
