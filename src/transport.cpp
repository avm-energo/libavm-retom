#include "libavm-retom/transport.h"

#include <QSerialPortInfo>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace retom {

QString findPort(quint16 vid, quint16 pid) {
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (info.hasVendorIdentifier() && info.hasProductIdentifier() &&
            info.vendorIdentifier() == vid && info.productIdentifier() == pid) {
            return info.portName();
        }
    }
    throw std::runtime_error("РЕТОМ FTDI не найден среди serial-портов (устройство не подключено, "
                              "либо занято другим процессом/драйвером)");
}

FtdiTransport::FtdiTransport(quint16 vid, quint16 pid, QString port)
    : vid_(vid), pid_(pid), port_(std::move(port)) {}

void FtdiTransport::open(qint32 baud) {
    const QString port = port_.isEmpty() ? findPort(vid_, pid_) : port_;
    serial_.setPortName(port);
    serial_.setBaudRate(baud);
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);
    if (!serial_.open(QIODevice::ReadWrite)) {
        throw std::runtime_error(("не удалось открыть порт " + port + ": " +
                                   serial_.errorString()).toStdString());
    }
    // pyserial по умолчанию поднимает DTR/RTS при открытии порта; QSerialPort -- нет (состояние
    // не определено). Выравниваем поведение явно -- расхождение с pyserial-версией транспорта.
    const bool dtrOk = serial_.setDataTerminalReady(true);
    const bool rtsOk = serial_.setRequestToSend(true);
    if (std::getenv("RETOM_DEBUG"))
    {
        std::fprintf(stderr,
            "[transport] port=%s dtrOk=%d rtsOk=%d pinoutSignals=0x%x lastError=%s\n",
            port.toStdString().c_str(), dtrOk, rtsOk, static_cast<unsigned>(serial_.pinoutSignals()),
            serial_.errorString().toStdString().c_str());
    }
}

void FtdiTransport::close() {
    if (serial_.isOpen()) {
        serial_.close();
    }
}

bool FtdiTransport::isOpen() const {
    return serial_.isOpen();
}

qint64 FtdiTransport::write(const QByteArray &data) {
    const qint64 n = serial_.write(data);
    serial_.waitForBytesWritten(1000);
    return n;
}

QByteArray FtdiTransport::read(int maxSize, int timeoutMs) {
    if (serial_.bytesAvailable() < maxSize) {
        serial_.waitForReadyRead(timeoutMs);
    }
    return serial_.read(maxSize);
}

void FtdiTransport::bindToIoThread(QThread *thread) {
    serial_.moveToThread(thread);
}

}  // namespace retom
