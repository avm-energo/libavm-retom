#include "libavm-retom/driver.h"

#include "libavm-retom/framing.h"
#include "libavm-retom/protocol.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
// Общий для процесса таймер отсчёта для RETOM_DEBUG -- стартует лениво при первом
// debug-выводе, чтобы TX/RX строки на обеих платформах можно было сравнить по РЕАЛЬНЫМ
// интервалам между кадрами (2026-08-25 -- содержимое кадров идентично Windows/Linux, дальше
// проверять можно только тайминг).
double debugElapsedMs()
{
    static QElapsedTimer timer;
    static bool started = false;
    if (!started)
    {
        timer.start();
        started = true;
    }
    return timer.nsecsElapsed() / 1e6;
}

// RETOM_DEBUG=1 -- см. комментарий у transact(). Отдельная функция для fire-and-forget записей
// (Lock/probe), которые идут в обход transact() и не ждут ответа.
void debugTxRaw(const QByteArray &frame)
{
    if (std::getenv("RETOM_DEBUG"))
    {
        std::fprintf(stderr, "[%9.3f] TX(no-wait) %s\n", debugElapsedMs(), frame.toHex(' ').constData());
    }
}
} // namespace

namespace retom
{

RetomDriver::RetomDriver(RetomConfig cfg, std::unique_ptr<ITransport> transport)
    : cfg_(std::move(cfg))
    , transport_(transport ? std::move(transport) : std::make_unique<FtdiTransport>(cfg_.vid, cfg_.pid))
{
    // ioThread_ живёт весь срок жизни RetomDriver (не только между connect()/disconnect()) --
    // единственный поток, которому вообще разрешено трогать transport_ (см. driver.h). Настоящий
    // QThread (не std::thread) -- QSerialPort требует реальную Qt event loop у своего потока-
    // владельца, иначе падает на "Timers can only be used with threads started with QThread"
    // (проверено живьём 2026-08-25 при первой, неудачной попытке на голом std::thread).
    ioThread_.start();
    ioContext_ = new QObject();
    ioContext_->moveToThread(&ioThread_);
    transport_->bindToIoThread(&ioThread_);
}

RetomDriver::~RetomDriver()
{
    // std::thread (keepAliveThread_) не должен быть joinable() на момент разрушения (иначе
    // std::terminate) -- на случай, если вызывающий код уничтожит RetomDriver, не вызвав
    // off()/disconnect() явно, пока держится ненулевой сигнал. keepAlive останавливаем ПЕРВЫМ (он
    // сам зависит от ioThread_ через transact()), потом останавливаем сам ioThread_.
    stopKeepAlive();
    ioThread_.quit();
    ioThread_.wait();
    delete ioContext_; // ioThread_ уже не бежит -- безопасно удалить из любого потока
}

template <typename F>
auto RetomDriver::runOnIoThread(F &&f) -> std::invoke_result_t<F>
{
    using R = std::invoke_result_t<F>;
    std::exception_ptr eptr;
    if constexpr (std::is_void_v<R>)
    {
        QMetaObject::invokeMethod(
            ioContext_,
            [&f, &eptr]()
            {
                try
                {
                    f();
                } catch (...)
                {
                    eptr = std::current_exception();
                }
            },
            Qt::BlockingQueuedConnection);
        if (eptr)
        {
            std::rethrow_exception(eptr);
        }
    } else
    {
        R result {};
        QMetaObject::invokeMethod(
            ioContext_,
            [&f, &result, &eptr]()
            {
                try
                {
                    result = f();
                } catch (...)
                {
                    eptr = std::current_exception();
                }
            },
            Qt::BlockingQueuedConnection);
        if (eptr)
        {
            std::rethrow_exception(eptr);
        }
        return result;
    }
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
    runOnIoThread([this] { transport_->open(kDefaultBaud); });
    QThread::msleep(1000); // даём VCP/устройству осесть после открытия порта
    runOnIoThread(
        [this]
        {
            debugTxRaw(protocol::encodeLock(true));
            transport_->write(protocol::encodeLock(true));
        });
    QThread::msleep(50);

    try
    {
        // probe(0x0000)/0x0003 -- fire-and-forget, ответа не получают НИКОГДА (подтверждено живым
        // тестом 2026-08-18, блокер Фазы D). Ожидание ответа здесь и было причиной ложного
        // "зависания устройства" в исходной реализации.
        for (const QByteArray &frame : protocol::encodeConnectBootstrap())
        {
            runOnIoThread(
                [this, &frame]
                {
                    debugTxRaw(frame);
                    transport_->write(frame);
                });
        }
    } catch (...)
    {
        try
        {
            runOnIoThread([this] { transport_->write(protocol::encodeLock(false)); });
        } catch (...)
        {
        }
        runOnIoThread([this] { transport_->close(); });
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
            self->runOnIoThread([self = self] { self->transport_->close(); });
            self->connected_ = false;
            self->enabled_ = false;
            self->outputActive_ = false;
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
            runOnIoThread(
                [this, &frame]
                {
                    debugTxRaw(frame);
                    transport_->write(frame);
                });
        }
        else
        {
            transact(frame, std::max(cfg_.timeoutS, 2.0));
        }
    }
    // Снято из живого захвата второго пакета ПО (не-RTDI) -- см. bootstrap.cpp::kEnableCalibration.
    // Гипотеза: реальная аппаратная самокалибровка каналов, нужна раз за сессию питания прибора --
    // без неё apply() давал 0V, пока прибор не был "прогрет" сторонним ПО в этой же сессии питания.
    for (const QByteArray &frame : protocol::encodeEnableCalibration())
    {
        transact(frame, std::max(cfg_.timeoutS, 2.0));
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
    startKeepAlive();
    outputActive_ = true;
}

void RetomDriver::off()
{
    requireConnected();
    stopKeepAlive();
    outputActive_ = false;
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

void RetomDriver::startKeepAlive()
{
    stopKeepAlive(); // идемпотентный перезапуск -- на случай повторного apply() поверх активного
    keepAliveStop_ = false;
    keepAliveThread_ = std::thread(&RetomDriver::keepAliveLoop, this);
}

void RetomDriver::stopKeepAlive()
{
    if (keepAliveThread_.joinable())
    {
        {
            std::lock_guard<std::mutex> lock(keepAliveMutex_);
            keepAliveStop_ = true;
        }
        keepAliveCv_.notify_all();
        keepAliveThread_.join();
    }
}

void RetomDriver::keepAliveLoop()
{
    // 2с -- взято из живого захвата второго пакета ПО (опрос каждые ~3.1с не давал прибору
    // "остыть"; 2с даёт запас). condition_variable вместо sleep() -- чтобы stopKeepAlive() будил
    // поток немедленно, а не ждал до 2с на выходе из off()/disconnect().
    std::unique_lock<std::mutex> lock(keepAliveMutex_);
    while (!keepAliveCv_.wait_for(lock, std::chrono::seconds(2), [this] { return keepAliveStop_; }))
    {
        lock.unlock();
        try
        {
            transact(protocol::encodeReadInputContacts());
        } catch (...)
        {
            // best-effort -- транзиентная ошибка протокола не должна ронять фоновый поток.
        }
        lock.lock();
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
    // Вся функция выполняется на ioThread_ (см. driver.h -- QSerialPort не переживает вызовы из
    // чужого потока) -- вызывающий поток (в т.ч. keepAliveLoop()) блокируется внутри
    // runOnIoThread() (Qt::BlockingQueuedConnection) до готовности результата/исключения.
    // RETOM_DEBUG=1 в окружении -- дамп каждого TX/RX кадра на stderr (диагностика проблем,
    // воспроизводимых только на конкретном стенде, где нет живого доступа для отладки, например
    // 2026-08-25 -- Linux-стенд показывает 0V при чистом (без исключений) прогоне, см. RE_LOG.md).
    const bool debug = std::getenv("RETOM_DEBUG") != nullptr;
    return runOnIoThread(
        [this, &frame, timeoutS, debug]() -> std::pair<quint16, QByteArray>
        {
            if (debug)
            {
                std::fprintf(stderr, "[%9.3f] TX %s\n", debugElapsedMs(), frame.toHex(' ').constData());
            }
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
                    const auto result = framing::unpackFrame(buf.mid(start, end - start + 1));
                    if (debug)
                    {
                        std::fprintf(stderr, "[%9.3f] RX num=%#x payload=%s\n", debugElapsedMs(), result.first,
                            result.second.toHex(' ').constData());
                    }
                    return result;
                } catch (const framing::FramingError &)
                {
                    buf = buf.mid(start + 1);
                }
            }
            if (debug)
            {
                std::fprintf(stderr, "[%9.3f] RX TIMEOUT\n", debugElapsedMs());
            }
            throw RetomProtocolError(
                QString("нет валидного ответа от прибора за %1 с").arg(timeout, 0, 'f', 1).toStdString());
        });
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
