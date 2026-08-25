// Минимальный пример жизненного цикла RetomDriver: connect() -> ожидание -> disconnect().
// НЕ вызывает apply() -- не выдаёт аналоговый сигнал, поэтому безопасно запускать без
// согласования со стендом (в отличие от tools/live_smoke_test.cpp).
//
// Сборка: часть examples/ (см. examples/CMakeLists.txt, подключается опционально через
// RETOM_BUILD_EXAMPLES в корневом CMakeLists.txt, как и тесты/tools).
//
// Запуск: connect_wait_disconnect [порт]
//   Порт по умолчанию -- /dev/ttyUSB0 (Linux). На Windows передайте, например, COM10.
#include "libavm-retom/driver.h"

#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include <exception>

int main(int argc, char **argv)
{
    // Обязателен: RetomDriver маршалит весь доступ к последовательному порту на внутренний
    // выделенный поток через Qt::BlockingQueuedConnection (QSerialPort не переживает вызовы из
    // чужого потока, см. docs/RE_LOG.md) -- этому механизму нужен работающий QCoreApplication,
    // даже если сам пример не запускает event loop (app.exec() здесь не нужен).
    QCoreApplication app(argc, argv);

    const QString port = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("/dev/ttyUSB0");

    retom::RetomConfig cfg;
    retom::RetomDriver driver(cfg, std::make_unique<retom::FtdiTransport>(retom::kRetomVid, retom::kRetomPid, port));

    try
    {
        std::printf("connect() to %s ...\n", qPrintable(port));
        driver.connect(); // enable=true по умолчанию -- проходит самотест/калибровку прибора (~5-10с)
        std::printf("connected, waiting 60s ...\n");

        QThread::sleep(60);

        std::printf("disconnect()\n");
        driver.disconnect();

        std::printf("OK\n");
        return 0;
    } catch (const std::exception &e)
    {
        std::fprintf(stderr, "ERROR: %s\n", e.what());
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
