# libavm-retom

[![pipeline status](https://git.avmenergo.ru/avm-energo/libavm-retom/badges/master/pipeline.svg)](https://git.avmenergo.ru/avm-energo/libavm-retom/-/pipelines)
[![latest release](https://git.avmenergo.ru/avm-energo/libavm-retom/-/badges/release.svg)](https://git.avmenergo.ru/avm-energo/libavm-retom/-/releases)

English: [README.md](README.md)

**libavm-retom** — библиотека-драйвер для устройств испытательного стенда **РЕТОМ-51** производства
НПП «Динамика» (<https://dynamics.com.ru/>), для проектов АВМ-Энерго. Графический клиент —
[avm-retom](https://git.avmenergo.ru/avm-energo/avm-retom).

Программа разработана ООО «АВМ-Энерго» с помощью Claude.

Библиотека общается с устройством через его виртуальный COM-порт FTDI (`QSerialPort`, VID/PID `0403:6001`).

API реализует ровно то подмножество, которое подтверждено дампами: `connect`/`disconnect`, `enable`/`disable`,
`apply`/`off`/`safetyStop`, `readInputContacts`, `setOutContact`

# Требования

- Инструменты:
  - компилятор C++ с полной поддержкой C++17 (MSVC 2019 16.10+, gcc 9.3.0+, clang-11+)
  - CMake 3.18+
- Зависимости:
  - Qt 6 (Core + SerialPort) — https://www.qt.io/. `find_package(QT NAMES Qt6 Qt5)` откатывается на Qt 5, если
    Qt 6 не найден, но проверялся только Qt 6.

# Сборка

```bash
$ cmake -B build -S . -DCMAKE_PREFIX_PATH=путь_к_qt
$ cmake --build build --config Release
```

Поддерживаемые опции:
- cmake
  - `CMAKE_PREFIX_PATH` — путь к установке Qt, например `C:\Qt\6.10.1\msvc2022_64`
  - `BUILD_SHARED_LIBS:BOOL=ON/OFF` — динамическая (`.dll`/`.so`) или статическая (`.lib`/`.a`) библиотека.
    Если не передать `ON`, действует умолчание CMake (`OFF`, т. е. статическая).
  - `CMAKE_BUILD_TYPE` — `Debug`/`Release`/`MinSizeRel`/`RelWithDebInfo` (по умолчанию `Release`)
  - `RETOM_BUILD_TESTS:BOOL=ON/OFF` — собирать автономные юнит-тесты QtTest из `tests/` (по умолчанию `ON`)

`tests/` (юнит-тесты, запуск через `ctest`) и `tools/` (ручная проверка на живом оборудовании) собираются только
когда это корневой проект CMake — при подключении через `add_subdirectory()`/`FetchContent` их не будет.

# Использование

```cpp
#include <libavm-retom/driver.h>

retom::RetomConfig cfg;
retom::RetomDriver driver(cfg);  // настоящий FtdiTransport, поиск по VID/PID

driver.connect(/* enable = */ true);

retom::RetomSignalSet sigSet;
sigSet.u["A"] = 1.0;   // Ua = 1.0 В
sigSet.freq = 50.0;
driver.apply(sigSet);

driver.off();
driver.disconnect();
```

Если к стенду подключено несколько устройств FTDI, передайте явный `FtdiTransport` с COM-портом вместо поиска по
VID/PID:

```cpp
retom::RetomDriver driver(cfg, std::make_unique<retom::FtdiTransport>(
    retom::kRetomVid, retom::kRetomPid, "COM10"));
```

Любой вызов при ошибке бросает `retom::RetomError` (или подкласс — `RetomNotConnected`, `RetomNotEnabled`,
`RetomProtocolError`). Полный рабочий пример всего цикла connect/apply/off/setOutContact/disconnect на реальном
оборудовании — `tools/live_smoke_test.cpp`.

# Пакеты

Linux-пакеты собирает CI (`.gitlab-ci.yml`) через CPack: `libavm-retom` (runtime, динамическая библиотека) и
`libavm-retom-dev` (`.deb`) / `libavm-retom-devel` (`.rpm`) (статическая библиотека, заголовки, файлы CMake-пакета). Локальная сборка:

```bash
$ cmake -B build -S . -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr
$ cmake --build build
$ cd build && cpack -G DEB   # или -G RPM
```

Пакеты каждой версии (тега) перечислены на [странице релизов](https://git.avmenergo.ru/avm-energo/libavm-retom/-/releases).
