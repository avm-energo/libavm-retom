#pragma once

#include "libavm-retom/retom_export.h"

#include <QByteArray>
#include <QSerialPort>
#include <QString>

// Транспортный слой -- порт src/linretom/transport.py. Доступ через штатный VCP-порт FTDI
// (QSerialPort), не через D2XX напрямую -- см. transport.py docstring для обоснования.
namespace retom {

constexpr quint16 kRetomVid = 0x0403;
constexpr quint16 kRetomPid = 0x6001;
constexpr qint32 kDefaultBaud = 115200;  // подтверждено эмпирически, см. docs/RE_LOG.md

AVM_RETOM_EXPORT QString findPort(quint16 vid = kRetomVid, quint16 pid = kRetomPid);

// Интерфейс транспорта -- позволяет RetomDriver работать как с реальным FtdiTransport, так и с
// поддельным транспортом в offline-тестах (QtTest), не трогая живое железо.
class AVM_RETOM_EXPORT ITransport {
public:
    virtual ~ITransport() = default;
    virtual void open(qint32 baud = kDefaultBaud) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray &data) = 0;
    virtual QByteArray read(int maxSize, int timeoutMs) = 0;
};

class AVM_RETOM_EXPORT FtdiTransport : public ITransport {
public:
    // port -- явное имя COM-порта (напр. "COM10"), в обход поиска по vid/pid; нужно, когда на
    // стенде подключено больше одного устройства с тем же VID:PID (см. docs/RE_LOG.md 2026-08-19 --
    // на этом стенде появился второй, не РЕТОМ, FTDI-адаптер, и поиск по vid/pid стал неоднозначным).
    explicit FtdiTransport(quint16 vid = kRetomVid, quint16 pid = kRetomPid, QString port = {});

    void open(qint32 baud = kDefaultBaud) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;
    QByteArray read(int maxSize, int timeoutMs) override;

private:
    quint16 vid_;
    quint16 pid_;
    QString port_;
    QSerialPort serial_;
};

}  // namespace retom
