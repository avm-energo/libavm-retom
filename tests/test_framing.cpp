#include "libavm-retom/framing.h"

#include <QTest>

using namespace retom;

// Порт самопроверки src/linretom/framing.py (__main__): round-trip + инвариант суммы + эталонный
// кадр, плюс золотой вектор из живой пары pack_frame(22, b"") -> "aa 16 00 00 00 6a 7f 95".
class TestFraming : public QObject {
    Q_OBJECT

private slots:
    void referenceVector() {
        const QByteArray frame = framing::packFrame(22, {});
        QCOMPARE(frame, QByteArray::fromHex("aa160000006a7f95"));
    }

    void roundTrip_data() {
        QTest::addColumn<QByteArray>("payload");
        QTest::addColumn<quint16>("number");

        const QList<QByteArray> vectors = {
            QByteArray(), QByteArray::fromHex("00"), QByteArray::fromHex("7f"),
            QByteArray::fromHex("80"), QByteArray::fromHex("ffffff"),
            [] { QByteArray b(20, '\0'); for (int i = 0; i < 20; ++i) b[i] = char(i); return b; }(),
        };
        const QList<quint16> numbers = {0, 1, 22, 0x1234, 0x3FFF};

        for (const QByteArray &v : vectors) {
            for (quint16 n : numbers) {
                QTest::addRow("num=%d len=%d", n, v.size()) << v << n;
            }
        }
    }

    void roundTrip() {
        QFETCH(QByteArray, payload);
        QFETCH(quint16, number);

        const QByteArray frame = framing::packFrame(number, payload);
        QCOMPARE(static_cast<quint8>(frame.front()), framing::kStart);
        QCOMPARE(static_cast<quint8>(frame.back()), framing::kEnd);
        for (int i = 1; i < frame.size() - 1; ++i) {
            QVERIFY(static_cast<quint8>(frame[i]) <= 0x7F);
        }

        const auto body = frame.mid(1, frame.size() - 4);
        const int chk14 = (static_cast<quint8>(frame[frame.size() - 3]) & 0x7F) |
                           ((static_cast<quint8>(frame[frame.size() - 2]) & 0x7F) << 7);
        int sum = 0;
        for (char b : body) sum += static_cast<quint8>(b);
        QCOMPARE((sum + chk14) % framing::kChkMod, 0);

        const auto [num2, payload2] = framing::unpackFrame(frame);
        QCOMPARE(num2, number);
        QCOMPARE(payload2, payload);
    }

    void rejectsBadChecksum() {
        QByteArray frame = framing::packFrame(5, QByteArray::fromHex("0102"));
        frame[frame.size() - 3] = frame[frame.size() - 3] ^ char(0x01);  // испортить сумму
        QVERIFY_THROWS_EXCEPTION(framing::FramingError, framing::unpackFrame(frame));
    }
};

QTEST_APPLESS_MAIN(TestFraming)
#include "test_framing.moc"
