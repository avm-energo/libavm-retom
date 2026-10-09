// Ручной инструмент для живых проверок DUT: держит RETOM в одном состоянии заданное число
// секунд, пока что-то другое (avm-tester/tools/kiv_*) читает модуль по своим протоколам.
// Печатает "APPLY ..." сразу после apply(), чтобы вызывающий процесс мог дождаться именно
// этого момента, а не гадать с фиксированной задержкой.
//   retomhold <port> <seconds> [UA UB UC IA IB IC phiA phiB phiC freq uPhiA uPhiB uPhiC]
// Без сигнальных аргументов подключается и включается, но ничего не подаёт -- нулевое
// состояние. uPhi по умолчанию 0/-120/-240 не задаётся -- см. live_smoke_test.cpp для разбора
// симметричной трёхфазной раскладки, если понадобится.
#include "libavm-retom/driver.h"

#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString port = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("/dev/ttyUSB0");
    const int seconds = argc > 2 ? std::atoi(argv[2]) : 20;
    retom::RetomConfig cfg;
    retom::RetomDriver driver(cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, port));
    try
    {
        driver.connect(true);
        std::printf("READY connected=%d enabled=%d\n", int(driver.isConnected()), int(driver.isEnabled()));
        std::fflush(stdout);
        if (argc >= 13)
        {
            retom::RetomSignalSet s;
            for (int k = 0; k < 3; ++k)
            {
                s.u[k] = std::atof(argv[3 + k]);
                s.i[k] = std::atof(argv[6 + k]);
                s.phi[k] = std::atof(argv[9 + k]);
            }
            s.freq = std::atof(argv[12]);
            if (argc >= 16)
                for (int k = 0; k < 3; ++k)
                    s.uPhi[k] = std::atof(argv[13 + k]);
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
        QThread::sleep(static_cast<unsigned long>(seconds));
        driver.off();
        driver.disconnect();
        std::printf("DONE output off, disconnected\n");
        return 0;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "ERROR: %s\n", e.what());
        try { driver.safetyStop(); } catch (...) {}
        try { driver.disconnect(); } catch (...) {}
        return 1;
    }
}
