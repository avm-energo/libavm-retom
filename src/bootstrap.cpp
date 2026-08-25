#include "libavm-retom/bootstrap.h"

namespace retom::bootstrap {

const std::vector<std::pair<quint16, QByteArray>> kConnectBootstrap = {
    {0x0000, QByteArray::fromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f60616263")},
    {0x0003, QByteArray::fromHex("")},
};

const std::vector<std::pair<quint16, QByteArray>> kEnableBootstrap = {
    {0x0800, QByteArray::fromHex("")},
    {0x0804, QByteArray::fromHex("aaaaaaaaeeeeeeeeffffffff0300cccc000000")},
    {0x0803, QByteArray::fromHex("")},
    {0x0800, QByteArray::fromHex("")},
    {0x000f, QByteArray::fromHex("4c6f636b00")},
    {0x0804, QByteArray::fromHex("aaaaaaaaeeeeeeeeffffffff0300cccc000000")},
    {0x0803, QByteArray::fromHex("")},
    {0x0806, QByteArray::fromHex("4d494e492d00000000009600000032000000")},
    {0x0806, QByteArray::fromHex("4d494e492c00000000009600000032000000")},
    {0x0806, QByteArray::fromHex("4d494e490900")},
    {0x0806, QByteArray::fromHex("4d494e49110001000100")},
    {0x0806, QByteArray::fromHex("4d494e49110001010100")},
    {0x0806, QByteArray::fromHex("4d494e49110001020100")},
    {0x0806, QByteArray::fromHex("4d494e491200010100")},
    {0x0806, QByteArray::fromHex("4d494e490100")},
    {0x0806, QByteArray::fromHex("4d494e490100")},
    {0x0806, QByteArray::fromHex("4d494e490000")},
    {0x0806, QByteArray::fromHex("4d494e491900010001000100")},
    {0x0806, QByteArray::fromHex("4d494e491900010001000100")},
    {0x0806, QByteArray::fromHex("4d494e491c000000")},
    {0x0806, QByteArray::fromHex("4d494e491200000000")},
    {0x0806, QByteArray::fromHex("4d494e491900010001000100")},
    {0x0806, QByteArray::fromHex("4d494e491900010001000100")},
    {0x080f, QByteArray::fromHex("000000002310300018")},
    {0x080f, QByteArray::fromHex("01ea070811020a0419")},
    {0x0806, QByteArray::fromHex("4d494e492500")},
    {0x0806, QByteArray::fromHex("4d494e491d00")},
    {0x0803, QByteArray::fromHex("")},
    {0x0806, QByteArray::fromHex("4d494e49320000")},
    {0x0806, QByteArray::fromHex("4d494e493100")},
    {0x0806, QByteArray::fromHex("4d494e49330002000000")},
    {0x0806, QByteArray::fromHex("4d494e493400000000000100000001")},
    {0x0806, QByteArray::fromHex("4d494e493400010000000100000001")},
    {0x0806, QByteArray::fromHex("4d494e4935000000000000000000")},
    {0x0806, QByteArray::fromHex("4d494e4935000100000000000000")},
    {0x0806, QByteArray::fromHex("4d494e49320001")},
    {0x0806, QByteArray::fromHex("4d494e49360001000000")},
    {0x0806, QByteArray::fromHex("4d494e49360000000000")},
    {0x0806, QByteArray::fromHex("4d494e493100")},
    {0x0803, QByteArray::fromHex("")},
    {0x0805, QByteArray::fromHex("0001000000")},
    {0x0803, QByteArray::fromHex("")},
    {0x0801, QByteArray::fromHex("504c55470100")},
    {0x0803, QByteArray::fromHex("")},
    {0x0806, QByteArray::fromHex("4d494e492500")},
    {0x0900, QByteArray::fromHex("01")},
    {0x0901, QByteArray::fromHex("01cdcc4c3ecdcc4c3e")},
    {0x0813, QByteArray::fromHex("ff000101")},
    {0x0814, QByteArray::fromHex("0000")},
    {0x0814, QByteArray::fromHex("0100")},
    {0x0814, QByteArray::fromHex("0200")},
    {0x0961, QByteArray::fromHex("ff010101")},
    {0x0951, QByteArray::fromHex("0a00")},
    {0x0950, QByteArray::fromHex("0000304101")},
    {0x0880, QByteArray::fromHex("10010000")},
    {0x0880, QByteArray::fromHex("00100000")},
    {0x0880, QByteArray::fromHex("00001000")},
    {0x0880, QByteArray::fromHex("00000010")},
    {0x0804, QByteArray::fromHex("64000000000000000000000000000000010000")},
    {0x090b, QByteArray::fromHex("")},
    {0x0970, QByteArray::fromHex("00000000")},
    {0x0971, QByteArray::fromHex("cdcccc3d0101")},
    {0x0972, QByteArray::fromHex("0000000000000000000000000000000000000000")},
    {0x0806, QByteArray::fromHex("4d494e492a0005000000")},
    {0x0804, QByteArray::fromHex("64000000000000000000000000000000010000")},
    {0x0803, QByteArray::fromHex("")},
    {0x0950, QByteArray::fromHex("0000304101")},
    {0x0806, QByteArray::fromHex("4d494e492a000a000000")},
    {0x0804, QByteArray::fromHex("64000000000000000000000000000000010000")},
    {0x0806, QByteArray::fromHex("4d494e491c000000")},
    {0x0806, QByteArray::fromHex("4d494e490200")},
    {0x0811, QByteArray::fromHex("")},
    {0x0804, QByteArray::fromHex("64000000000000000000000000000000010000")},
    {0x0806, QByteArray::fromHex("4d494e491c000000")},
    {0x0806, QByteArray::fromHex("4d494e490200")},
    {0x0811, QByteArray::fromHex("")},
    {0x0813, QByteArray::fromHex("03000101")},
    {0x0813, QByteArray::fromHex("04000101")},
    {0x0813, QByteArray::fromHex("05000101")},
};

// Блок чтения калибровки каналов (0x0824 x8 + 0x0806 "MINI 0x20" x8, повторено дважды) -- снят из
// живого захвата ВТОРОГО пакета ПО (не RTDI) с ГЕНУИННО холодного старта (2026-08-25, мэйнс-питание
// снято/подано заново перед захватом, capture начат ДО первого Lock). Найден и в этом же положении
// (внутри собственного Enable()-эквивалента второй программы) в более раннем "тёплом" захвате
// 2026-08-19 -- т.е. это не случайность одной сессии. Гипотеза: реальная аппаратная самокалибровка
// каналов, которая должна отработать РАЗ ЗА СЕССИЮ ПИТАНИЯ прибора -- объясняет находку 2026-08-25:
// наш ENABLE_BOOTSTRAP (снят только с RTDI, без этого блока) давал apply() без напряжения на выходе,
// ПОКА второй пакет ПО не "прогревал" прибор первым в той же сессии питания -- после чего наш же
// драйвер (без этого блока) успешно выдавал напряжение несколько раз подряд, пока состояние не
// "остывало". Добавлено сюда, в ENABLE_BOOTSTRAP, чтобы наш enable() сам выполнял эту калибровку и
// не зависел от стороннего ПО. НЕ проверено живьём после добавления -- см. docs/RE_LOG.md.
const std::vector<std::pair<quint16, QByteArray>> kEnableCalibration = {
    {0x0805, QByteArray::fromHex("0001000000")},
    {0x0824, QByteArray::fromHex("00000101")},
    {0x0824, QByteArray::fromHex("01000101")},
    {0x0824, QByteArray::fromHex("00000001")},
    {0x0824, QByteArray::fromHex("00010001")},
    {0x0824, QByteArray::fromHex("00020001")},
    {0x0824, QByteArray::fromHex("01000001")},
    {0x0824, QByteArray::fromHex("01010001")},
    {0x0824, QByteArray::fromHex("01020001")},
    {0x0806, QByteArray::fromHex("4d494e4920000000")},
    {0x0806, QByteArray::fromHex("4d494e4920000001")},
    {0x0806, QByteArray::fromHex("4d494e4920000002")},
    {0x0806, QByteArray::fromHex("4d494e4920000003")},
    {0x0806, QByteArray::fromHex("4d494e4920000100")},
    {0x0806, QByteArray::fromHex("4d494e4920000101")},
    {0x0806, QByteArray::fromHex("4d494e4920000102")},
    {0x0806, QByteArray::fromHex("4d494e4920000103")},
    {0x0805, QByteArray::fromHex("0001000000")},
    {0x0824, QByteArray::fromHex("00000101")},
    {0x0824, QByteArray::fromHex("01000101")},
    {0x0824, QByteArray::fromHex("00000001")},
    {0x0824, QByteArray::fromHex("00010001")},
    {0x0824, QByteArray::fromHex("00020001")},
    {0x0824, QByteArray::fromHex("01000001")},
    {0x0824, QByteArray::fromHex("01010001")},
    {0x0824, QByteArray::fromHex("01020001")},
    {0x0806, QByteArray::fromHex("4d494e4920000000")},
    {0x0806, QByteArray::fromHex("4d494e4920000001")},
    {0x0806, QByteArray::fromHex("4d494e4920000002")},
    {0x0806, QByteArray::fromHex("4d494e4920000003")},
    {0x0806, QByteArray::fromHex("4d494e4920000100")},
    {0x0806, QByteArray::fromHex("4d494e4920000101")},
    {0x0806, QByteArray::fromHex("4d494e4920000102")},
    {0x0806, QByteArray::fromHex("4d494e4920000103")},
};

const std::vector<std::pair<quint16, QByteArray>> kOutPreamble = {
    {0x0831, QByteArray::fromHex("000000000000000000000000")},
    {0x0831, QByteArray::fromHex("000000000100000000000000")},
    {0x0831, QByteArray::fromHex("000000000200000000000000")},
    {0x0831, QByteArray::fromHex("000000000300000000000000")},
    {0x0831, QByteArray::fromHex("010000000000000000000000")},
    {0x0831, QByteArray::fromHex("010000000100000000000000")},
    {0x0831, QByteArray::fromHex("010000000200000000000000")},
    {0x0831, QByteArray::fromHex("010000000300000000000000")},
    {0x0831, QByteArray::fromHex("010000000400000000000000")},
    {0x0831, QByteArray::fromHex("010000000500000000000000")},
    {0x0813, QByteArray::fromHex("00000101")},
    {0x0813, QByteArray::fromHex("01000101")},
    {0x0813, QByteArray::fromHex("02000101")},
};

// Минимальная arm-последовательность перед реальным Out(Ua=1.0V,Ia=1.0A) -- снята из СВЕЖЕГО
// живого дифференциального захвата 2026-08-25 (второй пакет ПО, НЕ RTDI, сессия начата ПОСЛЕ
// Enable() -- захват стартовал только на операции Out(), это чистая delta без шума Enable()).
// ЗАМЕНЯЕТ два предыдущих неудачных варианта (пара 0x0950/0x0904; затем 143-кадровый блок из
// устаревшего захвата 2026-08-19) -- оба не дали напряжения на живом приборе. Этот блок содержит
// шаг, которого не было ни в одном из предыдущих: 0x0814x3 (выбор канала 0/1/2) перед 0x0813x3.
// Семантика большинства подкоманд не установлена -- byte-в-byte replay, как ENABLE_BOOTSTRAP.
const std::vector<std::pair<quint16, QByteArray>> kOutArmPreamble = {
    {0x0806, QByteArray::fromHex("4d494e49070000")},
    {0x0902, QByteArray::fromHex("")},
    {0x0815, QByteArray::fromHex("000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000")},
    {0x0806, QByteArray::fromHex("4d494e490200")},
    {0x0811, QByteArray::fromHex("")},
    {0x0806, QByteArray::fromHex("4d494e490200")},
    {0x0806, QByteArray::fromHex("4d494e492a0005000000")},
    {0x0923, QByteArray::fromHex("")},
    {0x0814, QByteArray::fromHex("0000")},
    {0x0814, QByteArray::fromHex("0100")},
    {0x0814, QByteArray::fromHex("0200")},
    {0x0813, QByteArray::fromHex("00000101")},
    {0x0813, QByteArray::fromHex("01000101")},
    {0x0813, QByteArray::fromHex("02000101")},
};

}  // namespace retom::bootstrap
