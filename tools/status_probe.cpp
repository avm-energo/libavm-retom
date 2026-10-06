// Диагностика 0 V (docs/SESSION_HANDOFF.md, 2026-10-05): раз в секунду печатает ответы
// RetomDriver::readStatus(). Фаза 1 -- без выдачи, фаза 2 -- apply(Ua, Ia=0.5 А).
// Использование: status_probe <port> recover   (попытки программного лечения)
//                 status_probe <port> [секунд_без_выдачи=20] [секунд_с_выдачей=20] [Ua=1.0]
#include "libavm-retom/driver.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <optional>
#include <cstdio>
#include <exception>
#include <string>

static void poll(retom::RetomDriver &driver, const char *phase, int seconds)
{
    for (int t = 0; t < seconds; ++t)
    {
        const auto r = driver.readStatus();
        std::printf("%s t=%2ds 1c=%s 1d=%s 02=%s\n", phase, t, r[0].toHex(' ').constData(),
            r[1].toHex(' ').constData(), r[2].toHex(' ').constData());
        std::fflush(stdout);
        QThread::sleep(1);
    }
}

// Режим "recover" -- пробует программные способы вернуть флаг готовности каскада напряжения из 0
// в 1, по нарастающей, и печатает флаг после каждого шага. Нужен, когда прибор УЖЕ в состоянии 0 В
// (см. docs/SESSION_HANDOFF.md); на здоровом приборе ничего не доказывает.
static void flagLine(retom::RetomDriver &driver, const char *step)
{
    const auto f = driver.voltageStageReady();
    std::printf("recover: after %-34s flag=%s\n", step, f ? (*f ? "1" : "0") : "?");
    std::fflush(stdout);
}

static int recover(const char *port)
{
    retom::RetomConfig cfg;
    retom::RetomDriver driver(
        cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, port));
    driver.connect(false);
    flagLine(driver, "connect(false)");
    driver.enable();
    flagLine(driver, "enable()");
    driver.disable();
    driver.enable();
    flagLine(driver, "disable()+enable()");
    driver.disconnect();
    QThread::sleep(5);
    driver.connect(true);
    flagLine(driver, "disconnect, 5s, connect(true)");
    driver.disable();
    QThread::sleep(10);
    driver.enable();
    flagLine(driver, "disable, 10s idle, enable");
    driver.disconnect();
    return 0;
}

// Режим "eager" -- воспроизводит поведение avm-tester сразу после включения питания: ждёт, пока
// прибор начнёт отвечать, и тут же enable() + apply() (проверку флага обходит). Затем -- мониторинг
// флага раз в минуту (read-only сессии), чтобы измерить, сколько держится состояние 0 В.
static const char *flagStr(const std::optional<bool> &f)
{
    return f ? (*f ? "1" : "0") : "?";
}

static int eager(const char *port, int monitorMinutes)
{
    QElapsedTimer clock;
    clock.start();
    auto stamp = [&] { return clock.elapsed() / 1000.0; };
    for (int attempt = 0;; ++attempt)
    {
        try
        {
            retom::RetomConfig cfg;
            cfg.checkVoltageStage = false;
            cfg.timeoutS = 0.5;
            retom::RetomDriver driver(
                cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, port));
            driver.connect(false);
            const auto f0 = driver.voltageStageReady(); // бросит, пока прибор не отвечает
            std::printf("[%7.1fs] first reply, attempt %d, flag=%s\n", stamp(), attempt, flagStr(f0));
            driver.enable();
            std::printf("[%7.1fs] enabled, flag=%s\n", stamp(), flagStr(driver.voltageStageReady()));
            retom::RetomSignalSet sig;
            sig.u = { 57.74, 0, 0 };
            sig.i = { 0.5, 0, 0 };
            sig.freq = 50.0;
            driver.apply(sig);
            std::printf("[%7.1fs] applied (check bypassed), flag=%s\n", stamp(), flagStr(driver.voltageStageReady()));
            std::fflush(stdout);
            QThread::sleep(8);
            driver.off();
            driver.disconnect();
            break;
        } catch (const std::exception &e)
        {
            if (attempt < 5 || attempt % 20 == 0)
                std::printf("[%7.1fs] attempt %d: %s\n", stamp(), attempt, e.what());
            std::fflush(stdout);
            if (attempt > 600) { std::printf("gave up\n"); return 1; }
        }
        QThread::msleep(300);
    }
    for (int m = 1; m <= monitorMinutes; ++m)
    {
        QThread::sleep(60);
        try
        {
            retom::RetomConfig cfg;
            retom::RetomDriver driver(
                cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, port));
            driver.connect(true);
            const auto f = driver.voltageStageReady();
            std::printf("[%7.1fs] monitor +%d min: flag=%s\n", stamp(), m, flagStr(f));
            std::fflush(stdout);
            driver.disconnect();
            if (f && *f)
                break;
        } catch (const std::exception &e)
        {
            std::printf("[%7.1fs] monitor +%d min: error %s\n", stamp(), m, e.what());
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: status_probe <port> [idle_s] [active_s] [Ua] [nocheck]\n");
        return 2;
    }
    if (argc >= 3 && std::string(argv[2]) == "eager")
    {
        return eager(argv[1], argc > 3 ? std::atoi(argv[3]) : 20);
    }
    if (argc == 3 && std::string(argv[2]) == "recover")
    {
        try { return recover(argv[1]); } catch (const std::exception &e) { std::fprintf(stderr, "ERROR: %s\n", e.what()); return 1; }
    }
    const int idle = argc > 2 ? std::atoi(argv[2]) : 20;
    const int active = argc > 3 ? std::atoi(argv[3]) : 20;
    const double ua = argc > 4 ? std::atof(argv[4]) : 1.0;

    retom::RetomConfig cfg;
    cfg.checkVoltageStage = !(argc > 5 && std::string(argv[5]) == "nocheck"); // 5-й арг.: обойти проверку флага
    retom::RetomDriver driver(
        cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, argv[1]));
    try
    {
        driver.connect(false);
        poll(driver, "connected", 3);
        driver.enable();
        poll(driver, "enabled  ", idle);
        if (active > 0)
        {
            retom::RetomSignalSet s;
            s.u = { ua, 0, 0 };
            s.i = { 0.5, 0, 0 };
            s.phi = { 0, 0, 0 };
            s.freq = 50.0;
            driver.apply(s);
            poll(driver, "applied  ", active);
            driver.off();
        }
        driver.disconnect();
    } catch (const std::exception &e)
    {
        std::fprintf(stderr, "ERROR: %s\n", e.what());
        try { driver.safetyStop(); driver.disconnect(); } catch (...) {}
        return 1;
    }
    return 0;
}
