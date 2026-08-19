#pragma once

#include "libavm-retom/retom_export.h"

#include <QByteArray>
#include <stdexcept>
#include <utility>

// Транспортное кадрирование РЕТОМ-51 -- порт src/linretom/framing.py (источник истины).
// Раскладка и алгоритм суммы описаны там; здесь -- только реализация. Экспортируется (не только
// driver.h/transport.h), т.к. tests/ обращаются к этим функциям напрямую (см. test_framing.cpp,
// test_protocol.cpp) -- на shared-сборке под MSVC без AVM_RETOM_EXPORT здесь линковка тестов падает.
namespace retom::framing {

constexpr quint8 kStart = 0xAA;
constexpr quint8 kEnd = 0x95;
constexpr int kChkMod = 1 << 14;

class AVM_RETOM_EXPORT FramingError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

AVM_RETOM_EXPORT QByteArray packFrame(quint16 number, const QByteArray &data = {});
AVM_RETOM_EXPORT std::pair<quint16, QByteArray> unpackFrame(const QByteArray &frame);

}  // namespace retom::framing
