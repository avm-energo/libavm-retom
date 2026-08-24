#pragma once

#include "libavm-retom/retom_export.h"
#include "libavm-retom/transport.h"

#include <memory>
#include <stdexcept>
#include <utility>

// Публичный API драйвера РЕТОМ-51 -- порт src/linretom/driver.py (источник истины: семантика,
// ограничения и обоснования см. там же и в docs/RE_LOG.md). Масштаб -- то же подмножество, что
// подтверждено живыми USB-захватами: connect/disconnect, enable/disable, apply/off/safety_stop,
// read_input_contacts, set_out_contact. server_info() не реализован (номер команды не подтверждён).
namespace retom
{

struct AVM_RETOM_EXPORT RetomConfig
{
    QString retomType = QStringLiteral("RetomType_51");
    double maxU = 120.0;   // предел напряжения, В (проверяется на этой стороне, не на приборе)
    double maxI = 20.0;    // предел тока, А (на выходе РЕТОМ)
    double timeoutS = 1.0; // таймаут ожидания ответа на один кадр
    quint16 vid = kRetomVid;
    quint16 pid = kRetomPid;
};

// Набор U/I/phi/f по фазам "A"/"B"/"C" -- отсутствующая фаза трактуется как 0.0, как в apply().
struct AVM_RETOM_EXPORT RetomSignalSet
{
    std::array<double, 3> u;
    std::array<double, 3> i;
    std::array<double, 3> phi;
    double freq;
};

class AVM_RETOM_EXPORT RetomError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class AVM_RETOM_EXPORT RetomNotConnected : public RetomError
{
public:
    RetomNotConnected() : RetomError("Не подключён (вызовите connect()).")
    {
    }
};

class AVM_RETOM_EXPORT RetomNotEnabled : public RetomError
{
public:
    RetomNotEnabled() : RetomError("Аналоговая выдача не разрешена (вызовите enable()).")
    {
    }
};

class AVM_RETOM_EXPORT RetomProtocolError : public RetomError
{
public:
    using RetomError::RetomError;
};

class AVM_RETOM_EXPORT RetomDriver
{
public:
    // transport == nullptr -> создаётся настоящий FtdiTransport(cfg.vid, cfg.pid). Передача
    // своей реализации ITransport (например, в тестах) не трогает живое железо.
    explicit RetomDriver(RetomConfig cfg = {}, std::unique_ptr<ITransport> transport = nullptr);

    void connect(bool enable = true);
    void disconnect();

    void enable();
    void disable();

    void apply(const RetomSignalSet &signalSet);
    void off();
    void safetyStop();

    quint32 readInputContacts();
    void setOutContact(int number, bool closed);

    bool isConnected() const
    {
        return connected_;
    }
    bool isEnabled() const
    {
        return enabled_;
    }

private:
    std::pair<quint16, QByteArray> transact(const QByteArray &frame, double timeoutS = -1.0);
    void sendOut(const std::array<double, 13> &signalWords13);
    void requireConnected() const;
    void requireEnabled() const;

    RetomConfig cfg_;
    std::unique_ptr<ITransport> transport_;
    bool connected_ = false;
    bool enabled_ = false;
};

} // namespace retom
