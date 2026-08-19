#pragma once

#include "libavm-retom/retom_export.h"

#include <QByteArray>
#include <QString>
#include <array>
#include <map>
#include <optional>
#include <utility>
#include <vector>

// Прикладной протокол RTDI<->РЕТОМ -- порт src/linretom/protocol.py (источник истины и
// обоснование раскладки полей/номеров команд -- см. docs/PROTOCOL.md, docs/RE_LOG.md). Экспортируется
// (не только driver.h/transport.h), т.к. tests/ обращаются к этим функциям напрямую -- см. framing.h.
namespace retom::protocol {

constexpr quint16 CMD_SINE_ALL = 0x815;   // 12xFN: все 6 каналов {ампл, фаза}
constexpr quint16 CMD_SINE_U = 0x816;     // 6xFN: напряжения U a/b/c {ампл, фаза}
constexpr quint16 CMD_SINE_I = 0x817;     // 6xFN: токи I a/b/c {ампл, фаза}
constexpr quint16 CMD_FREQ = 0x818;       // DW@0, FN@4=Гц, BT@8/9/a
constexpr quint32 kMaskAllChannels = 0x770077;

constexpr quint16 CMD_LOCK = 0x00F;                  // захват/освобождение прибора
constexpr quint16 CMD_LINK_PROBE = 0x000;            // проба канала сразу после Lock
constexpr quint16 CMD_LINK_ACK = 0x003;              // пустой кадр сразу после пробы
constexpr quint16 CMD_READ_INPUT_CONTACTS = 0x902;
constexpr quint16 CMD_SET_OUT_CONTACT_MINI = 0x806;
constexpr quint16 CMD_SET_OUT_CONTACT_880 = 0x880;
constexpr quint16 CMD_DISABLE = 0x812;

constexpr double kSqrt2 = 1.4142135623730951;  // RMS -> амплитуда (пик)

// Кадр 0x815: amps_phases = 12 значений Ua,phi_a, Ub,phi_b, Uc,phi_c, Ia,phi_a, Ib,phi_b, Ic,phi_c
// (без частоты). Раскладка подтверждена живыми захватами -- см. docs/RE_LOG.md.
AVM_RETOM_EXPORT QByteArray encodeSineSet(const std::array<double, 12> &ampsPhases);

// Кадр 0x818: DW@0 = floor(10000/freqHz) по умолчанию (переопределяется через mode),
// FN@4 = freqHz без масштаба, BT@8/9/0xA = flags (по умолчанию статические 1,1,0x11).
AVM_RETOM_EXPORT QByteArray encodeFrequency(double freqHz, std::optional<quint32> mode = std::nullopt,
                            std::array<quint8, 3> flags = {1, 1, 0x11});

// signalWords13 = [Ua,phi_a, Ub,phi_b, Uc,phi_c, Ia,phi_a, Ib,phi_b, Ic,phi_c, f]
AVM_RETOM_EXPORT std::vector<QByteArray> encodeOut(const std::array<double, 13> &signalWords13);
AVM_RETOM_EXPORT std::vector<QByteArray> encodeChannelsReset();

// u/i/phi -- значения по фазам "A"/"B"/"C" (отсутствующая фаза = 0.0). Ток -- в А на выходе РЕТОМ.
AVM_RETOM_EXPORT std::array<double, 13> buildSignalWords(const std::map<QString, double> &u,
                                          const std::map<QString, double> &i,
                                          const std::map<QString, double> &phi, double freq);

AVM_RETOM_EXPORT QByteArray encodeLock(bool openDevice);

// probe(0x0000) + пустой(0x0003) -- fire-and-forget, ответа не получают НИКОГДА (подтверждено
// живым тестом 2026-08-18, см. docs/RE_LOG.md, блокер Фазы D). Вызывающий код не должен ждать
// ответа на эти кадры.
AVM_RETOM_EXPORT std::vector<QByteArray> encodeConnectBootstrap();

AVM_RETOM_EXPORT std::vector<QByteArray> encodeEnableBootstrap();
// Как encodeEnableBootstrap(), но с номером команды при каждом кадре -- нужно driver.cpp, чтобы
// отличить кадры, ожидающие ответа, от повторного Lock (CMD_LOCK) внутри последовательности,
// который ответа не получает (подтверждено живым тестом 2026-08-18).
AVM_RETOM_EXPORT std::vector<std::pair<quint16, QByteArray>> encodeEnableBootstrapTagged();

AVM_RETOM_EXPORT std::vector<QByteArray> encodeOutPreamble();

AVM_RETOM_EXPORT QByteArray encodeReadInputContacts();
AVM_RETOM_EXPORT quint32 decodeInputContacts(const QByteArray &payload);

AVM_RETOM_EXPORT std::vector<QByteArray> encodeSetOutContact(bool closed, int number = 0);

AVM_RETOM_EXPORT QByteArray encodeDisable();

}  // namespace retom::protocol
