// Ручной инструмент для живой проверки C++-порта на реальном приборе (Фаза E.7, PLAN.md).
// Зеркалит Python-проверку 2026-08-18 (docs/RE_LOG.md, "apply()/set_out_contact() подтверждены"):
// connect(enable=true) -> apply(Ua=1.0В,f=50Гц) -> удержание -> off() -> setOutContact(0,True/False)
// -> disconnect(). Требует согласованный со стендом безопасный запуск (см. capture/README.md) --
// не входит в ctest/CI, запускается вручную.
#include "libavm-retom/driver.h"

#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include <exception>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const int holdSeconds = argc > 1 ? std::atoi(argv[1]) : 8;
    const double ua = argc > 2 ? std::atof(argv[2]) : 1.0;

    // Явный COM10 -- на стенде теперь два FTDI-устройства (docs/RE_LOG.md 2026-08-19), поиск по
    // vid/pid неоднозначен; COM10 -- подтверждённый РЕТОМ.
    retom::RetomConfig cfg;
    retom::RetomDriver driver(
        cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, "/dev/ttyUSB0"));
    try
    {
        std::printf("connect(enable=true) ...\n");
        driver.connect(true);
        std::printf("connected+enabled OK\n");

        retom::RetomSignalSet sigSet;
        sigSet.u = { ua, 0, 0 };
        sigSet.i = { 1, 0, 0 };
        sigSet.phi = { 0, 0, 0 };
        sigSet.freq = 50.0;

        std::printf("apply(Ua=%.2fV, f=50Hz) -- holding %ds ...\n", ua, holdSeconds);
        driver.apply(sigSet);
        QThread::sleep(static_cast<unsigned long>(holdSeconds));

        std::printf("off()\n");
        driver.off();

        std::printf("setOutContact(0, true)\n");
        driver.setOutContact(0, true);
        QThread::msleep(1000);
        std::printf("setOutContact(0, false)\n");
        driver.setOutContact(0, false);

        std::printf("disconnect()\n");
        driver.disconnect();

        std::printf("\n+++ LIVE C++ SMOKE TEST OK, NO RTDI +++\n");
        return 0;
    } catch (const std::exception &e)
    {
        std::fprintf(stderr, "\nERROR: %s\n", e.what());
        if (driver.isConnected())
        {
            try
            {
                driver.disconnect();
            } catch (...)
            {
            }
        }
        return 1;
    }
}
