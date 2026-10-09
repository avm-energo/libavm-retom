// Ручной инструмент для живых проверок DUT: держит RETOM в одном состоянии заданное число
// секунд, пока что-то другое (avm-tester/tools/kiv_*) читает модуль по своим протоколам.
// Печатает "APPLY ..." сразу после apply(), чтобы вызывающий процесс мог дождаться именно
// этого момента, а не гадать с фиксированной задержкой.
//   retomhold <port> <seconds> [UA UB UC IA IB IC phiA phiB phiC freq uPhiA uPhiB uPhiC]
// Без сигнальных аргументов подключается и включается, но ничего не подаёт -- нулевое
// состояние. uPhi по умолчанию 0/-120/-240.
//
// phiA/phiB/phiC -- угол КАЖДОЙ фазы тока ОТНОСИТЕЛЬНО СВОЕГО ЖЕ напряжения
// (т.е. относительно uPhi той же фазы), не абсолютный фазовый угол. Это
// единственное безопасное значение по умолчанию: если задать одно и то же
// "абсолютное" значение для всех трёх фаз при разнесённых uPhi (симметричная
// трёхфазная сеть), все три токовых фазора схлопнутся в одно направление
// вместо разнесения на 120° -- RetomSignalSet.phi сама по себе абсолютна,
// схлопывание тока происходит незаметно (величины на выходе RETOM и на входах
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
