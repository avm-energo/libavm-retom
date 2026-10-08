#include "libavm-retom/driver.h"
#include "libavm-retom/framing.h"
#include "libavm-retom/protocol.h"

#include <QTest>
#include <algorithm>
#include <memory>

using namespace retom;

namespace
{

// Имитация живого прибора по фактам, подтверждённым живыми тестами 2026-08-18/19 (docs/RE_LOG.md,
// "ФАЗА D РЕШЕНА" и "✅ РЕШЕНО: восстановление без RTDI"): Lock (ЛЮБОЙ -- и первый, и повторный, и
// Unlock) НИКОГДА не получает ответа -- подтверждено и для RTDI, и для второго пакета ПО
// производителя, ни разу за целую живую сессию, включая рабочие циклы с реальной аналоговой
// выдачей. probe(0x0000)/0x0003 -- тоже без ответа. Всё остальное отвечает.
class FakeTransport : public ITransport
{
public:
    void open(qint32) override
    {
        opened_ = true;
    }
    void close() override
    {
        opened_ = false;
    }
    bool isOpen() const override
    {
        return opened_;
    }

    qint64 write(const QByteArray &frame) override
    {
        writtenFrames_.push_back(frame);
        const auto [num, payload] = framing::unpackFrame(frame);

        if (num == protocol::CMD_LOCK || num == protocol::CMD_LINK_PROBE || num == protocol::CMD_LINK_ACK)
        {
            return frame.size(); // fire-and-forget, ответа не бывает никогда
        }
        if (num == protocol::CMD_SET_OUT_CONTACT_MINI && payload.startsWith(QByteArray("MINI\x1c", 5)))
        {
            pendingRead_ += framing::packFrame(num, statusReply_); // ответ на запрос флага готовности
            return frame.size();
        }
        pendingRead_ += framing::packFrame(num, QByteArray(1, '\x01'));
        return frame.size();
    }

    QByteArray read(int maxSize, int) override
    {
        const QByteArray chunk = pendingRead_.left(maxSize);
        pendingRead_.remove(0, chunk.size());
        return chunk;
    }

    // 12 байт: флаг готовности каскада напряжения -- байт 11 (см. RetomDriver::voltageStageReady()).
    QByteArray statusReply_ = QByteArray::fromHex("010101000000000300000001");
    bool opened_ = false;
    QByteArray pendingRead_;
    std::vector<QByteArray> writtenFrames_;
};

} // namespace

class TestDriver : public QObject
{
    Q_OBJECT

private slots:
    // Регрессионный тест на блокер Фазы D + фикс 2026-08-19 (docs/RE_LOG.md): connect()/enable() не
    // должны блокирующе ждать ответа на Lock (ни первый, ни повторный)/probe(0x0000)/0x0003 --
    // FakeTransport их не отвечает, поэтому если реализация регрессирует к transact()-ожиданию на
    // этих кадрах, тест либо упадёт с RetomProtocolError, либо ощутимо замедлится (см. короткий
    // timeoutS ниже).
    void connectEnableFullLifecycle()
    {
        auto fake = std::make_unique<FakeTransport>();
        FakeTransport *fakePtr = fake.get();

        RetomConfig cfg;
        cfg.timeoutS = 0.2; // короткий -- любой регрессивный blocking-wait быстро провалит тест
        RetomDriver driver(cfg, std::move(fake));

        QVERIFY_THROWS_NO_EXCEPTION(driver.connect(true));
        QVERIFY(driver.isConnected());
        QVERIFY(driver.isEnabled());

        const bool sawProbe = std::any_of(fakePtr->writtenFrames_.begin(), fakePtr->writtenFrames_.end(),
            [](const QByteArray &f) { return framing::unpackFrame(f).first == protocol::CMD_LINK_PROBE; });
        QVERIFY(sawProbe);

        RetomSignalSet sigSet;
        sigSet.u[0] = 1.0;
        sigSet.freq = 50.0;
        QVERIFY_THROWS_NO_EXCEPTION(driver.apply(sigSet));
        QVERIFY_THROWS_NO_EXCEPTION(driver.off());

        QVERIFY_THROWS_NO_EXCEPTION(driver.setOutContact(0, true));
        QVERIFY_THROWS_NO_EXCEPTION(driver.setOutContact(0, false));

        QVERIFY_THROWS_NO_EXCEPTION(driver.disconnect());
        QVERIFY(!driver.isConnected());
        QVERIFY(!driver.isEnabled());
    }

    void guardsRejectUseBeforeConnect()
    {
        RetomConfig cfg;
        RetomDriver driver(cfg, std::make_unique<FakeTransport>());
        QVERIFY_THROWS_EXCEPTION(RetomNotConnected, driver.readInputContacts());
    }

    void guardsRejectApplyBeforeEnable()
    {
        RetomConfig cfg;
        cfg.timeoutS = 0.2;
        RetomDriver driver(cfg, std::make_unique<FakeTransport>());
        driver.connect(false); // подключён, но не enable()

        RetomSignalSet sigSet;
        QVERIFY_THROWS_EXCEPTION(RetomNotEnabled, driver.apply(sigSet));
    }

    // До 2026-10-08 apply() сверялся с этим флагом и бросал RetomVoltageStageNotReady -- убрано:
    // живой захват показал, что сам флаг (0x1c) читается "готов" лишь секунду-другую после
    // enable(), затем сам откатывается на 0 независимо от того, реально ли прибор способен выдать
    // напряжение (он способен, стабильно, каждый раз -- проверено Энергомонитором). Опрос статуса
    // перед apply() сам по себе и был причиной ложного "не готов": лишний обмен кадрами выводил за
    // то самое окно. apply() больше не проверяет флаг и не блокируется им -- voltageStageReady()
    // остаётся самостоятельным диагностическим методом (см. docs/SESSION_HANDOFF.md, 2026-10-08).
    void applyProceedsRegardlessOfVoltageStageFlag()
    {
        RetomConfig cfg;
        cfg.timeoutS = 0.2;
        auto fake = std::make_unique<FakeTransport>();
        fake->statusReply_ = QByteArray::fromHex("010101000000000300000000"); // флаг 0
        RetomDriver driver(cfg, std::move(fake));
        driver.connect(true);

        QCOMPARE(driver.voltageStageReady(), std::optional<bool>(false)); // метод сам по себе не меняется

        RetomSignalSet withU;
        withU.u[0] = 57.74;
        QVERIFY_THROWS_NO_EXCEPTION(driver.apply(withU));
        QVERIFY(driver.isOutputActive());
        driver.off();
    }

    void applyPassesWhenVoltageStageReady()
    {
        RetomConfig cfg;
        cfg.timeoutS = 0.2;
        RetomDriver driver(cfg, std::make_unique<FakeTransport>());
        driver.connect(true);

        RetomSignalSet withU;
        withU.u[0] = 57.74;
        driver.apply(withU);
        QVERIFY(driver.isOutputActive());
        driver.off();
    }

    void applyRejectsOverLimit()
    {
        RetomConfig cfg;
        cfg.timeoutS = 0.2;
        RetomDriver driver(cfg, std::make_unique<FakeTransport>());
        driver.connect(true);

        RetomSignalSet sigSet;
        sigSet.u[0] = cfg.maxU + 1.0;
        QVERIFY_THROWS_EXCEPTION(RetomError, driver.apply(sigSet));
    }
};

// QTEST_MAIN (не APPLESS) -- RetomDriver теперь требует настоящий QCoreApplication: ioThread_ /
// ioContext_ используют Qt::BlockingQueuedConnection для маршалинга вызовов на выделенный поток
// (QSerialPort не переживает вызовы из чужого потока, см. driver.h/docs/RE_LOG.md, решено
// 2026-08-25), а этот механизм сам по себе требует QCoreApplication, даже если тест использует
// только FakeTransport и не трогает живой QSerialPort.
QTEST_MAIN(TestDriver)
#include "test_driver.moc"
