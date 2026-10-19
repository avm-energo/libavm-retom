// Ручной инструмент для живых проверок DUT: держит RETOM в одном состоянии заданное число
// секунд, пока что-то другое (avm-tester/tools/kiv_*) читает модуль по своим протоколам.
// Печатает "APPLY ..." сразу после apply(), чтобы вызывающий процесс мог дождаться именно
// этого момента, а не гадать с фиксированной задержкой.
//   retomhold <port> <seconds> [UA UB UC IA IB IC phiA phiB phiC freq uPhiA uPhiB uPhiC]
//   retomhold <port> check   -- one-shot voltage-output pre-flight check, see below
// Без сигнальных аргументов подключается и включается, но ничего не подаёт -- нулевое
// состояние. uPhi по умолчанию 0/-120/-240.
//
// phiA/phiB/phiC -- угол КАЖДОЙ фазы тока ОТНОСИТЕЛЬНО СВОЕГО ЖЕ напряжения
// (т.е. относительно uPhi той же фазы), не абсолютный фазовый угол. Это
// единственное безопасное значение по умолчанию: если задать одно и то же
// "абсолютное" значение для всех трёх фаз при разнесённых uPhi (симметричная
// трёхфазная сеть), все три токовых фазора схлопнутся в одно направление
// вместо разнесения на 120° -- RetomSignalSet.phi сама по себе абсолютна,
// схлопывание тока происходит незаметно (величины на выходе РЕТОМ и на входах
// ДУТ остаются верными), но ломает всё, что физически зависит от взаимного
// разнесения токов по фазе: расчёт небаланса токов (Iunb), симметричные
// составляющие и т.п. -- найдено вживую 10 октября 2026 (см.
// avm-tester/ai/DECISIONS.md, "A bench-side bug, not a DUT one"): тест на
// Iunb с тем же числом для phiA/phiB/phiC давал ~318% вместо документированных
// 20% именно по этой причине, ток был полностью верным по амплитуде, но три
// фазных тока совпадали по направлению. Эта программа теперь всегда
// складывает phi[k] с uPhi[k] сама -- вызывающему достаточно указать один и
// тот же угол (например, 89,8281°) для всех трёх фаз, если нужен одинаковый
// cosφ на каждой фазе, не вычисляя абсолютные углы вручную.
//
// ИСКЛЮЧИТЕЛЬНЫЙ ДОСТУП К ПОРТУ (добавлено 19 октября 2026, avm-tester):
// до этой правки ничто не мешало двум запускам retomhold на один порт работать
// одновременно -- ни один из них не получал чистой ошибки "порт занят", но оба
// вместе создавали непредсказуемое поведение устройства (перепутанные кадры,
// неверный ответ на команды вроде Sbg), которое выглядит как сбой самого ДУТа
// или даже РЕТОМа, а на деле -- забытый фоновый процесс держит порт. Поймано
// живьём: ручная диагностика оставила retomhold в фоне на 100с, а следующий
// вызов из автоматического теста получил Sbg: Error response без видимой
// причины -- `pgrep -af retomhold` нашёл висящий процесс, kill сразу всё
// починил. Теперь: эксклюзивная блокировка файла `/tmp/retomhold-<port>.lock`
// (flock, неблокирующая) берётся до открытия самого порта -- если занято,
// программа сразу и понятно говорит, каким PID, вместо того чтобы дать второй
// процесс тихо мешать первому.
//
// GRACEFUL SIGTERM (добавлено туда же): раньше единственный способ прервать
// retomhold раньше заданного времени -- kill -9, который пропускает
// driver.off()/disconnect() и может оставить РЕТОМ посреди выдачи. Теперь
// обычный `kill` (SIGTERM) или Ctrl-C (SIGINT) прерывают ожидание и всё равно
// проходят через штатное выключение перед выходом.
//
// РЕЖИМ "check" (добавлено туда же): `retomhold <port> check` -- однократная
// предполётная проверка (RetomDriver::verifyVoltageOutput()): подаёт реальный
// тестовый сигнал по фазе A, ждёт установления, проверяет, что напряжение на
// клеммах действительно есть, и печатает чёткий PASS/FAIL вместо того, чтобы
// предоставлять это диагностировать вручную через Энергомонитор каждый раз.
// Мотивировано тем же сбоем, что и остальные правки в этом файле: втроём за
// одну неделю (15/16/19 октября 2026) найден живой, воспроизводимый отказ
// каскада напряжения РЕТОМ (ток и частота в норме, напряжения на клеммах
// нет), каждый раз требовавший ручной диагностики Энергомонитором, прежде
// чем стало ясно, что дело не в ДУТе и не в тестовом инструменте.
#include "libavm-retom/driver.h"

#include <QCoreApplication>
#include <QThread>

#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace
{
volatile sig_atomic_t g_stopRequested = 0;

void handleStopSignal(int)
{
    g_stopRequested = 1;
}

/// A filesystem-safe stand-in for a port path -- "/dev/ttyUSB4" becomes
/// "_dev_ttyUSB4", good enough as a lock-file name, not meant to be unique
/// beyond that.
QString sanitizedForFilename(const QString &port)
{
    QString out = port;
    for (QChar &c : out)
    {
        if (!c.isLetterOrNumber())
            c = QLatin1Char('_');
    }
    return out;
}

/// Exclusive, non-blocking flock() on a lock file keyed by the port path, so
/// a second retomhold on the SAME port fails loudly and immediately instead
/// of silently contending with the first one. The returned fd must be kept
/// open (not closed) for the lock's lifetime -- flock() releases
/// automatically when the fd closes, including on process exit or crash, so
/// there is nothing to clean up explicitly.
int acquirePortLock(const QString &port)
{
    const QString lockPath = QStringLiteral("/tmp/retomhold-%1.lock").arg(sanitizedForFilename(port));
    const int fd = ::open(lockPath.toLocal8Bit().constData(), O_CREAT | O_RDWR, 0666);
    if (fd < 0)
    {
        std::fprintf(stderr, "ERROR: could not open lock file %s\n", lockPath.toLocal8Bit().constData());
        return -1;
    }
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
    {
        char heldBy[64] = {};
        const ssize_t n = ::read(fd, heldBy, sizeof(heldBy) - 1);
        std::fprintf(stderr,
            "ERROR: port %s is already held by another retomhold (lock file %s%s%s) -- not opening it, to "
            "avoid silently contending with whatever already has it. Find it with `pgrep -af retomhold` and "
            "kill it (plain `kill`, not -9) if it's stale.\n",
            port.toLocal8Bit().constData(), lockPath.toLocal8Bit().constData(), n > 0 ? ", pid=" : "",
            n > 0 ? heldBy : "");
        ::close(fd);
        return -1;
    }
    // Record our own PID so a stuck lock is easy to diagnose by hand.
    if (::ftruncate(fd, 0) == 0)
    {
        const QByteArray pidText = QByteArray::number(static_cast<qint64>(::getpid()));
        if (::write(fd, pidText.constData(), pidText.size()) < 0)
        { /* best-effort diagnostic only */
        }
    }
    return fd;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString port = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("/dev/ttyUSB0");
    const bool checkMode = argc > 2 && QString::fromLocal8Bit(argv[2]) == QLatin1String("check");
    const int seconds = (argc > 2 && !checkMode) ? std::atoi(argv[2]) : 20;

    const int lockFd = acquirePortLock(port);
    if (lockFd < 0)
        return 1;

    std::signal(SIGTERM, handleStopSignal);
    std::signal(SIGINT, handleStopSignal);

    retom::RetomConfig cfg;
    retom::RetomDriver driver(cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, port));

    if (checkMode)
    {
        try
        {
            driver.connect(true);
            std::printf("checking RETOM voltage output on %s (applying a real single-phase test signal, "
                        "settling, then verifying)...\n",
                port.toLocal8Bit().constData());
            std::fflush(stdout);
            driver.verifyVoltageOutput();
            driver.disconnect();
            std::printf("PASS: RETOM voltage output confirmed\n");
            ::close(lockFd);
            return 0;
        }
        catch (const std::exception &e)
        {
            std::fprintf(stderr, "FAIL: %s\n", e.what());
            try { driver.safetyStop(); } catch (...) {}
            try { driver.disconnect(); } catch (...) {}
            ::close(lockFd);
            return 1;
        }
    }

    try
    {
        driver.connect(true);
        std::printf("READY connected=%d enabled=%d\n", int(driver.isConnected()), int(driver.isEnabled()));
        std::fflush(stdout);
        if (argc >= 13)
        {
            retom::RetomSignalSet s;
            // Default to a proper symmetric three-phase voltage, not
            // RetomSignalSet's own {0,0,0} -- the common case, and the only
            // way the per-phase-relative phi[] below means what it says
            // unless the caller overrides uPhi explicitly.
            s.uPhi = { 0.0, -120.0, -240.0 };
            if (argc >= 16)
                for (int k = 0; k < 3; ++k)
                    s.uPhi[k] = std::atof(argv[13 + k]);
            for (int k = 0; k < 3; ++k)
            {
                s.u[k] = std::atof(argv[3 + k]);
                s.i[k] = std::atof(argv[6 + k]);
                // phi[k] as given is relative to this phase's own uPhi[k],
                // not an absolute phasor angle -- see the header comment.
                s.phi[k] = s.uPhi[k] + std::atof(argv[9 + k]);
            }
            s.freq = std::atof(argv[12]);
            std::printf("APPLY U=%.4g/%.4g/%.4g uPhi=%.4g/%.4g/%.4g I=%.4g/%.4g/%.4g phi=%.4g/%.4g/%.4g f=%.4g\n",
                s.u[0], s.u[1], s.u[2], s.uPhi[0], s.uPhi[1], s.uPhi[2],
                s.i[0], s.i[1], s.i[2], s.phi[0], s.phi[1], s.phi[2], s.freq);
            std::fflush(stdout);
            driver.apply(s);
        }
        else
        {
            std::printf("ZERO no signal applied\n");
            std::fflush(stdout);
        }
        // Sleep in short slices rather than one QThread::sleep(seconds), so
        // a SIGTERM/SIGINT can break out promptly and still reach the
        // graceful off()/disconnect() below instead of needing -9.
        for (int waited = 0; waited < seconds && !g_stopRequested; ++waited)
            QThread::sleep(1);
        if (g_stopRequested)
        {
            std::printf("INTERRUPTED stopping early, still shutting down cleanly\n");
            std::fflush(stdout);
        }
        driver.off();
        driver.disconnect();
        std::printf("DONE output off, disconnected\n");
        ::close(lockFd);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "ERROR: %s\n", e.what());
        try { driver.safetyStop(); } catch (...) {}
        try { driver.disconnect(); } catch (...) {}
        ::close(lockFd);
        return 1;
    }
}
