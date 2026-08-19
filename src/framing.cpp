#include "libavm-retom/framing.h"

namespace retom::framing {

namespace {

// Точная модель цикла sub_0x10a0 (RWFULink): acc = data[i] | (data[i+1]<<8);
// emit (acc>>bitOff)&0x7F; bitOff += 7; i += bitOff>>3; bitOff &= 7.
QByteArray pack7(const QByteArray &data) {
    QByteArray out;
    const int n = data.size();
    int src = 0;
    int bitoff = 0;
    while (src < n) {
        int acc = static_cast<quint8>(data[src]);
        if (src + 1 < n) {
            acc |= static_cast<quint8>(data[src + 1]) << 8;
        }
        out.append(static_cast<char>((acc >> bitoff) & 0x7F));
        bitoff += 7;
        src += bitoff >> 3;
        bitoff &= 7;
    }
    return out;
}

QByteArray unpack7(const QByteArray &groups, int length) {
    quint32 bits = 0;
    int nbits = 0;
    QByteArray out;
    for (char g : groups) {
        bits |= (static_cast<quint32>(static_cast<quint8>(g)) & 0x7F) << nbits;
        nbits += 7;
        while (nbits >= 8 && out.size() < length) {
            out.append(static_cast<char>(bits & 0xFF));
            bits >>= 8;
            nbits -= 8;
        }
    }
    return out.left(length);
}

}  // namespace

QByteArray packFrame(quint16 number, const QByteArray &data) {
    if (number > 0x3FFF) {
        throw FramingError("номер команды вне 14 бит");
    }
    if (data.size() > 0x1004) {
        throw FramingError("нагрузка > 0x1004 байт");
    }
    const QByteArray payload = pack7(data);
    const int pc = payload.size();

    QByteArray body;
    body.append(static_cast<char>(number & 0x7F));
    body.append(static_cast<char>((number >> 7) & 0x7F));
    body.append(static_cast<char>(pc & 0x7F));
    body.append(static_cast<char>((pc >> 7) & 0x7F));
    body += payload;

    int sum = 0;
    for (char b : body) {
        sum += static_cast<quint8>(b);
    }
    sum &= (kChkMod - 1);
    const int chk = (-sum) & (kChkMod - 1);

    QByteArray frame;
    frame.append(static_cast<char>(kStart));
    frame += body;
    frame.append(static_cast<char>(chk & 0x7F));
    frame.append(static_cast<char>((chk >> 7) & 0x7F));
    frame.append(static_cast<char>(kEnd));
    return frame;
}

std::pair<quint16, QByteArray> unpackFrame(const QByteArray &frame) {
    if (frame.size() < 8 || static_cast<quint8>(frame[0]) != kStart ||
        static_cast<quint8>(frame[frame.size() - 1]) != kEnd) {
        throw FramingError("кадр: неверные делимитеры/длина");
    }
    const QByteArray body = frame.mid(1, frame.size() - 4);  // номер+длина+нагрузка
    const quint8 chkLo = static_cast<quint8>(frame[frame.size() - 3]);
    const quint8 chkHi = static_cast<quint8>(frame[frame.size() - 2]);
    const int chk14 = (chkLo & 0x7F) | ((chkHi & 0x7F) << 7);

    int sum = 0;
    for (char b : body) {
        sum += static_cast<quint8>(b);
    }
    if ((sum + chk14) & (kChkMod - 1)) {
        throw FramingError("кадр: контрольная сумма не сходится");
    }

    const quint16 number = (static_cast<quint8>(body[0]) & 0x7F) |
                            ((static_cast<quint8>(body[1]) & 0x7F) << 7);
    const int pc = (static_cast<quint8>(body[2]) & 0x7F) |
                    ((static_cast<quint8>(body[3]) & 0x7F) << 7);
    const QByteArray groups = body.mid(4, pc);
    const int length = (pc * 7) / 8;
    return {number, unpack7(groups, length)};
}

}  // namespace retom::framing
