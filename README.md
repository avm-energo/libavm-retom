# libavm-retom

Driver library for the РЕТОМ-51 relay test-bench device, for AVM-Energo
projects. Talks to the device over its FTDI virtual COM port (`QSerialPort`,
VID/PID `0403:6001`) using a reverse-engineered framing/command protocol -
this project has no vendor documentation to work from, only Windows
USB captures of the vendor's own `RTDI.exe`/`RTDI.DualServer` COM server.
See the sibling `linretom` repository's `docs/RE_LOG.md` and `PLAN.md` for
the full reverse-engineering history, including every protocol detail
confirmed (or still open) against real hardware.

The API surface implements exactly the subset confirmed by live USB
captures so far: `connect`/`disconnect`, `enable`/`disable`,
`apply`/`off`/`safetyStop`, `readInputContacts`, `setOutContact` (contacts 0-3,
confirmed with a voltmeter on 2026-11-19). Higher-level
RTDI functionality (e.g. `ServerInfo`) is not implemented - its command
numbers are not yet confirmed; see `linretom`'s `PLAN.md` "Phase F" for
what remains.

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

# Known limitations

- `Lock` frames (device capture/release, sent on `connect()`/`disconnect()`/
  internally by `enable()`) never receive a response from the device -
  confirmed against both the vendor's RTDI and a second vendor software
  package, across an entire live session including real analog output
  cycles. This is expected device behavior, not a bug - `connect()`
  deliberately does not wait for one.
- Only the command subset listed above has been ported/verified; the rest
  of the RTDI COM API (`ServerInfo`, hardware output modes like
  `OutSteps`/`OutSeq`, harmonics) remains unreversed. See the `linretom`
  repository's `PLAN.md` for the current state and next steps.

# Packaging

`package/` holds Debian (`debian`/`debian-dev`) and RPM (`rpm/rpmbuild`)
packaging sources, matching the convention used across AVM-Energo's other
C++ libraries (see `libavm-gen`, `libavm-emon`).
