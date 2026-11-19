#include "libavm-retom/framing.h"
#include "libavm-retom/protocol.h"

#include <QTest>

using namespace retom;

// Золотые векторы -- байты живых USB-захватов и живых тестов 2026-08 (docs/RE_LOG.md), не
// синтетические. Ловят регрессии в раскладке полей, не только во framing.
class TestProtocol : public QObject
{
    Q_OBJECT

private slots:
    void lockFrameMatchesLiveCapture()
    {
        // Захвачено живьём 2026-08-18 (live_lock_probe.py) -- байты на проводе, включая контрольную сумму.
        QCOMPARE(protocol::encodeLock(true), QByteArray::fromHex("aa0f0006004c5e0d5b0600537d95"));
        QCOMPARE(protocol::encodeLock(false), QByteArray::fromHex("aa0f0006004c5e0d5b1600437d95"));
    }

    void frequencyPayloadMatchesFormula_data()
    {
        QTest::addColumn<double>("freqHz");
        QTest::addColumn<QByteArray>("expectedPayload");
        // DW@0=floor(10000/freq), FN@4=freq (float32 LE, без масштаба), BT@8/9/A=(1,1,0x11) --
        // подтверждено тремя живыми захватами (50/60/100 Гц), docs/RE_LOG.md.
        QTest::newRow("50Hz") << 50.0 << QByteArray::fromHex("c800000000004842010111");
        QTest::newRow("60Hz") << 60.0 << QByteArray::fromHex("a600000000007042010111");
        QTest::newRow("100Hz") << 100.0 << QByteArray::fromHex("640000000000c842010111");
    }

    void frequencyPayloadMatchesFormula()
    {
        QFETCH(double, freqHz);
        QFETCH(QByteArray, expectedPayload);
        const auto [num, payload] = framing::unpackFrame(protocol::encodeFrequency(freqHz));
        QCOMPARE(num, protocol::CMD_FREQ);
        QCOMPARE(payload, expectedPayload);
    }

    void sineSetScalesAmplitudeAndPlacesPhase()
    {
        // Ua=1.0В, phi_a=90 -- offset0..3=Ua*sqrt2 (float32 LE), offset4..7=90.0 (без масштаба).
        // Подтверждено живым захватом 2026-08-17 (capture/2026-08-17_out_ua1v_phi90.pcap).
        std::array<double, 12> ampsPhases {};
        ampsPhases[0] = 1.0;  // Ua
        ampsPhases[1] = 90.0; // phi_a
        const auto [num, payload] = framing::unpackFrame(protocol::encodeSineSet(ampsPhases));
        QCOMPARE(num, protocol::CMD_SINE_ALL);
        QCOMPARE(payload.size(), 0x60);
        QCOMPARE(payload.mid(0, 4), QByteArray::fromHex("f304b53f"));
        QCOMPARE(payload.mid(4, 4), QByteArray::fromHex("0000b442"));
        QCOMPARE(payload.mid(8), QByteArray(0x60 - 8, '\0')); // остальное -- нулевой хвост
    }

    void buildSignalWordsDefaultsMissingPhasesToZero()
    {
        const auto words = protocol::buildSignalWords({ { 1.0, 0.0, 0.0 } }, {}, { { 0.0, 2.0, 0.0 } }, {}, 50.0);
        QCOMPARE(words[0], 1.0);   // Ua
        QCOMPARE(words[1], 0.0);   // phi_a (uPhi не задан)
        QCOMPARE(words[6], 0.0);   // Ia (не задан)
        QCOMPARE(words[8], 2.0);   // Ib
        QCOMPARE(words[12], 50.0); // freq
    }

    void buildSignalWordsAppliesUPhi()
    {
        // Подтверждено живым фазометром 2026-08-25 (docs/SESSION_HANDOFF.md): uPhi={0,-120,-240}
        // -> phi(Ua-Ub)=phi(Ua-Uc)=phi(Ub-Uc)=0 на приборе (симметричная трёхфазная система).
        const auto words = protocol::buildSignalWords(
            { { 1.0, 2.0, 3.0 } }, { { 0.0, -120.0, -240.0 } }, { { 0.0, 0.0, 0.0 } }, {}, 50.0);
        QCOMPARE(words[1], 0.0);    // Ua_phi
        QCOMPARE(words[3], -120.0); // Ub_phi
        QCOMPARE(words[5], -240.0); // Uc_phi
    }

    void setOutContactEncodesFlag()
    {
        const auto frames = protocol::encodeSetOutContact(true, 0);
        QCOMPARE(frames.size(), 2);
        const auto [num0, payload0] = framing::unpackFrame(frames[0]);
        QCOMPARE(num0, protocol::CMD_SET_OUT_CONTACT_MINI);
        QCOMPARE(payload0, QByteArray::fromHex("4d494e49") + QByteArray::fromHex("05000001"));
        const auto [num1, payload1] = framing::unpackFrame(frames[1]);
        QCOMPARE(num1, protocol::CMD_SET_OUT_CONTACT_880);
        QCOMPARE(payload1, QByteArray::fromHex("11010000"));
    }

    void setOutContactNumberGoesIntoMini()
    {
        // Confirmed on hardware 2026-11-19: contacts 1..3 take the number in MINI byte 6, no 0x880 frame.
        for (int n = 1; n <= 3; ++n)
        {
            const auto frames = protocol::encodeSetOutContact(true, n);
            QCOMPARE(frames.size(), 1);
            const auto [num, payload] = framing::unpackFrame(frames[0]);
            QCOMPARE(num, protocol::CMD_SET_OUT_CONTACT_MINI);
            QCOMPARE(payload, QByteArray::fromHex("4d494e490500") + QByteArray(1, char(n)) + QByteArray::fromHex("01"));
        }
        const auto open = protocol::encodeSetOutContact(false, 2);
        QCOMPARE(framing::unpackFrame(open[0]).second, QByteArray::fromHex("4d494e4905000200"));
    }
};

QTEST_APPLESS_MAIN(TestProtocol)
#include "test_protocol.moc"
