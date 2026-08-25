#include "libavm-retom/driver.h"

#include "libavm-retom/framing.h"
#include "libavm-retom/protocol.h"

#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
#include <cmath>

namespace retom
{

RetomDriver::RetomDriver(RetomConfig cfg, std::unique_ptr<ITransport> transport)
    : cfg_(std::move(cfg))
    , transport_(transport ? std::move(transport) : std::make_unique<FtdiTransport>(cfg_.vid, cfg_.pid))
{
}

void RetomDriver::connect(bool enable)
{
    // Lock отправляется БЕЗ ожидания ответа -- как и проба канала (encodeConnectBootstrap()) и
    // повторный Lock внутри enable(). Найдено живым тестом 2026-08-19 (docs/RE_LOG.md, "РЕШЕНО:
    // восстановление без RTDI"): Lock в принципе может не подтверждаться ответом (подтверждено и
    // для RTDI, и для второго пакета ПО производителя -- ни разу за сессию, включая рабочие циклы
    // с реальной аналоговой выдачей), при этом устройство полностью работоспособно на остальные
    // кадры. Прежний retry-с-переоткрытием-порта (kLockMaxAttempts) был ложным источником
    // "зависаний": драйвер сдавался, не дождавшись ответа, который в принципе не должен был прийти.
    transport_->open(kDefaultBaud);
    QThread::msleep(1000); // даём VCP/устройству осесть после открытия порта
    transport_->write(protocol::encodeLock(true));
    QThread::msleep(50);

    try
    {
        // probe(0x0000)/0x0003 -- fire-and-forget, ответа не получают НИКОГДА (подтверждено живым
        // тестом 2026-08-18, блокер Фазы D). Ожидание ответа здесь и было причиной ложного
        // "зависания устройства" в исходной реализации.
        for (const QByteArray &frame : protocol::encodeConnectBootstrap())
        {
            transport_->write(frame);
        }
    } catch (...)
    {
        try
        {
            transport_->write(protocol::encodeLock(false));
        } catch (...)
        {
        }
        transport_->close();
        throw;
    }
    connected_ = true;
    if (enable)
    {
        this->enable();
    }
}

void RetomDriver::disconnect()
{
    struct Cleanup
    {
        RetomDriver *self;
        ~Cleanup()
        {
            self->transport_->close();
            self->connected_ = false;
            self->enabled_ = false;
        }
    } cleanup { this };

    if (connected_)
    {
        safetyStop();
        try
        {
            transact(protocol::encodeLock(false));
        } catch (const RetomProtocolError &)
        {
        }
    }
}

void RetomDriver::enable()
{
    requireConnected();
    for (const auto &[num, frame] : protocol::encodeEnableBootstrapTagged())
    {
        if (num == protocol::CMD_LOCK)
        {
            // Повторный Lock внутри последовательности -- прибор уже захвачен этим же
            // соединением, ответа не получает (подтверждено живым тестом 2026-08-18).
            transport_->write(frame);
        }
        else
        {
            transact(frame, std::max(cfg_.timeoutS, 2.0));
        }
    }
    enabled_ = true;
}

void RetomDriver::disable()
{
    requireConnected();
    transact(protocol::encodeDisable());
    enabled_ = false;
}

void RetomDriver::apply(const RetomSignalSet &signalSet)
{
    requireEnabled();

    for (const auto i : signalSet.i)
    {
        if (i > cfg_.maxI)
            throw RetomError(QString("I=%1 превышает max_i=%2").arg(i).arg(cfg_.maxI).toStdString());
    }

    for (const auto u : signalSet.u)
    {
        if (std::abs(u) > cfg_.maxU)
        {
            throw RetomError(QString("U=%1 превышает max_u=%2").arg(u).arg(cfg_.maxU).toStdString());
        }
    }

    // Снято из живого захвата второго пакета ПО (не-RTDI) -- см. bootstrap.cpp::kOutArmPreamble.
    // Только перед РЕАЛЬНОЙ (не нулевой) выдачей, не на каждый sendOut() -- ~140 доп. кадров,
    // дублировать их на каждый off()/safetyStop() бессмысленно дорого.
    for (const QByteArray &frame : protocol::encodeOutArmPreamble())
    {
        transact(frame);
    }

    const auto words = protocol::buildSignalWords(signalSet.u, signalSet.i, signalSet.phi, signalSet.freq);
    sendOut(words);
}

void RetomDriver::off()
{
    requireConnected();
    std::array<double, 13> zeros {};
    zeros[12] = 50.0;
    sendOut(zeros);
}

void RetomDriver::safetyStop()
{
    requireConnected();
    off();
    off();
    if (enabled_)
    {
        disable();
    }
}

void RetomDriver::sendOut(const std::array<double, 13> &signalWords13)
{
    for (const QByteArray &frame : protocol::encodeOutPreamble())
    {
        transact(frame);
    }
    for (const QByteArray &frame : protocol::encodeOut(signalWords13))
    {
        transact(frame);
    }
}

quint32 RetomDriver::readInputContacts()
{
    requireConnected();
    const auto [num, payload] = transact(protocol::encodeReadInputContacts());
    Q_UNUSED(num);
    return protocol::decodeInputContacts(payload);
}

void RetomDriver::setOutContact(int number, bool closed)
{
    requireConnected();
    for (const QByteArray &frame : protocol::encodeSetOutContact(closed, number))
    {
        transact(frame);
    }
}

std::pair<quint16, QByteArray> RetomDriver::transact(const QByteArray &frame, double timeoutS)
{
    transport_->write(frame);
    const double timeout = timeoutS < 0.0 ? cfg_.timeoutS : timeoutS;

    QElapsedTimer timer;
    timer.start();
    QByteArray buf;
    while (timer.elapsed() < timeout * 1000)
    {
        const int remainingMs = static_cast<int>(timeout * 1000 - timer.elapsed());
        const QByteArray chunk = transport_->read(128, std::min(remainingMs, 200));
        if (chunk.isEmpty())
        {
            continue;
        }
        buf += chunk;
        const int start = buf.indexOf(static_cast<char>(framing::kStart));
        if (start < 0)
        {
            buf.clear();
            continue;
        }
        const int end = buf.indexOf(static_cast<char>(framing::kEnd), start + 1);
        if (end < 0)
        {
            continue;
        }
        try
        {
            return framing::unpackFrame(buf.mid(start, end - start + 1));
        } catch (const framing::FramingError &)
        {
            buf = buf.mid(start + 1);
        }
    }
    throw RetomProtocolError(QString("нет валидного ответа от прибора за %1 с").arg(timeout, 0, 'f', 1).toStdString());
}

void RetomDriver::requireConnected() const
{
    if (!connected_)
    {
        throw RetomNotConnected();
    }
}

void RetomDriver::requireEnabled() const
{
    requireConnected();
    if (!enabled_)
    {
        throw RetomNotEnabled();
    }
}

} // namespace retom
