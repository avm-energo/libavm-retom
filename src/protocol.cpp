#include "libavm-retom/protocol.h"

#include "libavm-retom/bootstrap.h"
#include "libavm-retom/framing.h"

#include <cstring>

namespace retom::protocol
{

namespace
{

    void putFloat(QByteArray &buf, int offset, double value)
    {
        const float v = static_cast<float>(value);
        std::memcpy(buf.data() + offset, &v, sizeof(float));
    }

    void putU32(QByteArray &buf, int offset, quint32 value)
    {
        std::memcpy(buf.data() + offset, &value, sizeof(quint32));
    }

} // namespace

QByteArray encodeSineSet(const std::array<double, 12> &ampsPhases)
{
    // 12 полей x 4Б (шаг 4, плотная упаковка) + 48-байтный неизменно нулевой хвост (48+48=96) --
    // раскладка подтверждена живыми захватами, см. docs/RE_LOG.md.
    QByteArray buf(0x60, '\0');
    for (int k = 0; k < 12; ++k)
    {
        const bool isAmplitude = (k % 2 == 0); // Ua,Ub,Uc,Ia,Ib,Ic на чётных индексах
        putFloat(buf, 4 * k, isAmplitude ? ampsPhases[k] * kSqrt2 : ampsPhases[k]);
    }
    return framing::packFrame(CMD_SINE_ALL, buf);
}

QByteArray encodeFrequency(double freqHz, std::optional<quint32> mode, std::array<quint8, 3> flags)
{
    QByteArray buf(0xB, '\0');
    const quint32 dw0 = mode ? *mode : static_cast<quint32>(10000.0 / freqHz);
    putU32(buf, 0, dw0);
    putFloat(buf, 4, freqHz);
    buf[8] = static_cast<char>(flags[0]);
    buf[9] = static_cast<char>(flags[1]);
    buf[0xA] = static_cast<char>(flags[2]);
    return framing::packFrame(CMD_FREQ, buf);
}

std::vector<QByteArray> encodeOut(const std::array<double, 13> &signalWords13)
{
    std::array<double, 12> ampsPhases {};
    std::copy(signalWords13.begin(), signalWords13.begin() + 12, ampsPhases.begin());
    return { encodeSineSet(ampsPhases), encodeFrequency(signalWords13[12]) };
}

std::vector<QByteArray> encodeChannelsReset()
{
    std::array<double, 13> zeros {};
    zeros[12] = 50.0;
    return encodeOut(zeros);
}

std::array<double, 13> buildSignalWords(const std::array<double, 3> &u, const std::array<double, 3> &uPhi,
    const std::array<double, 3> &i, const std::array<double, 3> &phi, const double freq)
{
    std::array<double, 13> out {};
    int idx = 0;
    for (int count = 0; count < 3; ++count)
    {
        out[idx++] = u[count];
        out[idx++] = uPhi[count];
    }
    for (int count = 0; count < 3; ++count)
    {
        out[idx++] = i[count];
        out[idx++] = phi[count];
    }
    out[12] = freq;
    return out;
}

QByteArray encodeLock(bool openDevice)
{
    QByteArray data("Lock", 4);
    data.append(static_cast<char>(openDevice ? 0 : 1));
    return framing::packFrame(CMD_LOCK, data);
}

std::vector<QByteArray> encodeConnectBootstrap()
{
    std::vector<QByteArray> out;
    out.reserve(bootstrap::kConnectBootstrap.size());
    for (const auto &[num, data] : bootstrap::kConnectBootstrap)
    {
        out.push_back(framing::packFrame(num, data));
    }
    return out;
}

std::vector<QByteArray> encodeEnableBootstrap()
{
    std::vector<QByteArray> out;
    out.reserve(bootstrap::kEnableBootstrap.size());
    for (const auto &[num, data] : bootstrap::kEnableBootstrap)
    {
        out.push_back(framing::packFrame(num, data));
    }
    return out;
}

std::vector<QByteArray> encodeEnableCalibration()
{
    std::vector<QByteArray> out;
    out.reserve(bootstrap::kEnableCalibration.size());
    for (const auto &[num, data] : bootstrap::kEnableCalibration)
    {
        out.push_back(framing::packFrame(num, data));
    }
    return out;
}

std::vector<std::pair<quint16, QByteArray>> encodeEnableBootstrapTagged()
{
    std::vector<std::pair<quint16, QByteArray>> out;
    out.reserve(bootstrap::kEnableBootstrap.size());
    for (const auto &[num, data] : bootstrap::kEnableBootstrap)
    {
        out.emplace_back(num, framing::packFrame(num, data));
    }
    return out;
}

std::vector<QByteArray> encodeOutPreamble()
{
    std::vector<QByteArray> out;
    out.reserve(bootstrap::kOutPreamble.size());
    for (const auto &[num, data] : bootstrap::kOutPreamble)
    {
        out.push_back(framing::packFrame(num, data));
    }
    return out;
}

std::vector<QByteArray> encodeOutArmPreamble()
{
    std::vector<QByteArray> out;
    out.reserve(bootstrap::kOutArmPreamble.size());
    for (const auto &[num, data] : bootstrap::kOutArmPreamble)
    {
        out.push_back(framing::packFrame(num, data));
    }
    return out;
}

QByteArray encodeReadInputContacts()
{
    return framing::packFrame(CMD_READ_INPUT_CONTACTS, {});
}

quint32 decodeInputContacts(const QByteArray &payload)
{
    if (payload.size() != 4)
    {
        throw framing::FramingError("ожидалось 4 байта ответа ReadInputContacts");
    }
    quint32 value = 0;
    std::memcpy(&value, payload.constData(), sizeof(quint32));
    return value;
}

std::vector<QByteArray> encodeSetOutContact(bool closed, int number)
{
    // Подтверждено на живом приборе (2026-11-19, вольтметр на всех четырёх контактах):
    //  - номер контакта -- третий байт MINI-кадра: "MINI" 05 00 <number> <flag>. Для number=1,2,3
    //    замыкается именно этот контакт (раньше номер игнорировался, и любой запрос замыкал контакт 0).
    //  - контакт 0 одним MINI-кадром (05 00 00 flag) НЕ управляется -- им управляет сопровождающий
    //    кадр 0x880: [0x10|flag, 01, 00, 00] (хвост 0x11 сам замыкает контакт 0 при любом номере в MINI).
    //  Поэтому для number == 0 шлются оба кадра (как раньше), для number >= 1 -- только MINI, чтобы
    //    не трогать состояние контакта 0.
    const quint8 flag = closed ? 1 : 0;

    QByteArray mini("MINI", 4);
    mini.append(static_cast<char>(0x05));
    mini.append(static_cast<char>(0));
    mini.append(static_cast<char>(number));
    mini.append(static_cast<char>(flag));

    std::vector<QByteArray> frames { framing::packFrame(CMD_SET_OUT_CONTACT_MINI, mini) };
    if (number == 0)
    {
        QByteArray tail;
        tail.append(static_cast<char>(closed ? 0x11 : 0x10));
        tail.append(static_cast<char>(0x01));
        tail.append(static_cast<char>(0));
        tail.append(static_cast<char>(0));
        frames.push_back(framing::packFrame(CMD_SET_OUT_CONTACT_880, tail));
    }
    return frames;
}

QByteArray encodeDisable()
{
    return framing::packFrame(CMD_DISABLE, {});
}

} // namespace retom::protocol
