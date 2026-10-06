#pragma once

#include "libavm-retom/retom_export.h"
#include "libavm-retom/transport.h"

#include <QThread>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

// Публичный API драйвера РЕТОМ-51 -- порт src/linretom/driver.py (источник истины: семантика,
// ограничения и обоснования см. там же и в docs/RE_LOG.md). Масштаб -- то же подмножество, что
// подтверждено живыми USB-захватами: connect/disconnect, enable/disable, apply/off/safety_stop,
// read_input_contacts, set_out_contact. server_info() не реализован (номер команды не подтверждён).
namespace retom
{

struct AVM_RETOM_EXPORT RetomConfig
{
    QString retomType = QStringLiteral("RetomType_51");
    double maxU = 120.0;   // предел напряжения, В (проверяется на этой стороне, не на приборе)
    double maxI = 20.0;    // предел тока, А (на выходе РЕТОМ)
    double timeoutS = 1.0; // таймаут ожидания ответа на один кадр
    quint16 vid = kRetomVid;
    quint16 pid = kRetomPid;
    // apply() с ненулевым напряжением сверяется с флагом готовности выходного каскада и бросает
    // RetomVoltageStageNotReady вместо молчаливых 0 В (см. RetomDriver::voltageStageReady()).
    bool checkVoltageStage = true;
};

// Набор U/I/phi/f по фазам "A"/"B"/"C" -- отсутствующая фаза трактуется как 0.0, как в apply().
// Default member initializers обязательны: до 7bcabf1 (map->array) неуказанная фаза просто
// отсутствовала в std::map и lookup() отдавал 0.0; с std::array без инициализаторов
// `RetomSignalSet s; s.u[0] = 1.0;` (частое использование, см. tests/test_driver.cpp) оставляет
// s.u[1]/s.u[2]/s.i/s.phi/s.freq неопределённым мусором стека -- нашёл живой regression-тест
// (2026-08-25) при добавлении keepalive-потока: тот же тестовый код внезапно начал падать на
// "U=-9.25596e+61" из-за сдвига раскладки стека, обнажившего давно существовавший баг.
//
// uPhi -- угол каждого канала напряжения (по умолчанию 0/0/0, как раньше жёстко зашитое в
// buildSignalWords поведение). phi -- угол каждого канала тока, как и прежде; для симметричной
// трёхфазной сети типичный набор -- uPhi={0,-120,-240} (сами напряжения разнесены по фазе) и
// phi={0,-120,-240} (если токи должны совпадать по фазе со своим напряжением, т.е. cosφ=1) --
// подтверждено живым фазометром 2026-08-25 (см. docs/SESSION_HANDOFF.md).
struct AVM_RETOM_EXPORT RetomSignalSet
{
    std::array<double, 3> u { 0.0, 0.0, 0.0 };
    std::array<double, 3> uPhi { 0.0, 0.0, 0.0 };
    std::array<double, 3> i { 0.0, 0.0, 0.0 };
    std::array<double, 3> phi { 0.0, 0.0, 0.0 };
    double freq = 50.0;
};

class AVM_RETOM_EXPORT RetomError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class AVM_RETOM_EXPORT RetomNotConnected : public RetomError
{
public:
    RetomNotConnected() : RetomError("Не подключён (вызовите connect()).")
    {
    }
};

class AVM_RETOM_EXPORT RetomNotEnabled : public RetomError
{
public:
    RetomNotEnabled() : RetomError("Аналоговая выдача не разрешена (вызовите enable()).")
    {
    }
};

class AVM_RETOM_EXPORT RetomProtocolError : public RetomError
{
public:
    using RetomError::RetomError;
};

// Прибор принимает все кадры без ошибок, ток выдаёт, но выходной каскад напряжения не готов -- на
// клеммах U нет. Наблюдалось 2026-10-05; лечится отключением питания прибора от сети (мэйнс
// power-cycle), иногда со 2-3 попытки; USB-переподключение, перезапуск программы и ожидание не помогают.
class AVM_RETOM_EXPORT RetomVoltageStageNotReady : public RetomError
{
public:
    RetomVoltageStageNotReady()
        : RetomError("Прибор РЕТОМ не выдаёт напряжение: выходной каскад напряжения не готов (ток выдаётся "
                     "нормально, напряжения на клеммах не будет). Выключите питание РЕТОМ от сети минимум на 10 "
                     "секунд, включите, подождите ~30 с и подключитесь снова. Если ошибка повторилась -- "
                     "повторите включение/выключение (иногда нужно 2-3 раза); ожидание и перезапуск программы "
                     "не помогают.")
    {
    }
};

class AVM_RETOM_EXPORT RetomDriver
{
public:
    // transport == nullptr -> создаётся настоящий FtdiTransport(cfg.vid, cfg.pid). Передача
    // своей реализации ITransport (например, в тестах) не трогает живое железо.
    explicit RetomDriver(RetomConfig cfg = {}, std::unique_ptr<ITransport> transport = nullptr);
    ~RetomDriver(); // останавливает фоновый keepalive-поток, если он ещё активен

    void connect(bool enable = true);
    void disconnect();

    void enable();
    void disable();

    // Живая аналоговая выдача отключается прибором сама по себе, если хост молчит на проводе
    // слишком долго -- похоже на watchdog, реальный порог не измерен эмпирически, но ~10-13с
    // молчания достаточно, живой захват второго пакета ПО показывает опрос каждые ~3.1с как
    // безопасный интервал (см. docs/RE_LOG.md). apply() сам поднимает фоновый поток, который
    // дергает readInputContacts() каждые 2с, пока держится ненулевой сигнал -- off()/disconnect()
    // его останавливают. Вызывающему коду НЕ нужно ничего опрашивать самому.
    void apply(const RetomSignalSet &signalSet);
    void off();
    void safetyStop();

    quint32 readInputContacts();

    // Диагностика (2026-10-05): сырые ответы 0x806 на MINI 0x1c / 0x1d / 0x02 -- те самые запросы,
    // что есть в enable-bootstrap. Флаг в ответе на 0x1c отличает рабочие прогоны от прогонов с 0 V
    // (см. docs/SESSION_HANDOFF.md). Ничего не включает, безопасно вызывать в любой момент после
    // connect().
    std::vector<QByteArray> readStatus();

    // Флаг готовности выходного каскада напряжения: 5-й байт первой записи в ответе на MINI 0x1c.
    // Во всех зафиксированных прогонах с напряжением он 1, во всех прогонах с 0 В -- 0 (подтверждено
    // Энергомонитором 2026-10-05: после mains power-cycle флаг стал 1 и Ua появилось). Корреляция,
    // не документированная семантика. nullopt -- ответ не удалось разобрать (флаг неизвестен).
    std::optional<bool> voltageStageReady();
    void setOutContact(int number, bool closed);

    bool isConnected() const
    {
        return connected_;
    }
    bool isEnabled() const
    {
        return enabled_;
    }
    // Клиентское отслеживание, НЕ живой опрос прибора -- true после apply(), false после
    // off()/safetyStop()/disconnect(). Протокол РЕТОМ-51 не даёт способа спросить у самого прибора
    // текущее состояние аналогового выхода (проверено по полному каталогу методов RTLink.dll,
    // docs/BINARY_RECON.md, и IAnalogOutputs, docs/API_SPEC.md -- там тоже только локальный
    // геттер/сеттер на объекте-конструкторе сигнала, не живой запрос к железу; RTDI, похоже, сам
    // работает так же). Если внешний код изменит выход в обход этого экземпляра RetomDriver, этот
    // флаг не узнает.
    bool isOutputActive() const
    {
        return outputActive_;
    }

private:
    std::pair<quint16, QByteArray> transact(const QByteArray &frame, double timeoutS = -1.0);
    void sendOut(const std::array<double, 13> &signalWords13);
    void requireConnected() const;
    void requireEnabled() const;

    // Фоновый watchdog-keepalive -- поднимается apply() после реальной (не нулевой) выдачи,
    // останавливается off()/disconnect() ДО закрытия порта. startKeepAlive()/stopKeepAlive()
    // вызываются только из потока-владельца RetomDriver (не из самого фонового потока), поэтому
    // join() в stopKeepAlive() даёт корректную синхронизацию без гонки на connected_/enabled_.
    void startKeepAlive();
    void stopKeepAlive();
    void keepAliveLoop();

    // QSerialPort (внутри FtdiTransport) требует, чтобы ЛЮБОЙ вызов делался из потока, которому
    // принадлежит объект (affinity) -- И чтобы у этого потока была настоящая Qt event loop (иначе
    // "Timers can only be used with threads started with QThread", проверено живьём 2026-08-25:
    // попытка сделать это через голый std::thread ловила именно эту ошибку). Поэтому ioThread_ --
    // настоящий QThread (run() по умолчанию = exec()), transport_ переносится на него через
    // ITransport::bindToIoThread()/QObject::moveToThread(), а ioContext_ -- пустой QObject,
    // живущий на ioThread_, служащий целью для QMetaObject::invokeMethod(...,
    // Qt::BlockingQueuedConnection) -- это и есть runOnIoThread(): вызывающий поток (в т.ч.
    // keepAliveLoop(), сам по себе обычный std::thread) блокируется до выполнения лямбды на
    // ioThread_ и получает результат/исключение обратно.
    template <typename F> auto runOnIoThread(F &&f) -> std::invoke_result_t<F>;

    RetomConfig cfg_;
    std::unique_ptr<ITransport> transport_;
    bool connected_ = false;
    bool enabled_ = false;
    bool outputActive_ = false;

    QThread ioThread_;
    QObject *ioContext_ = nullptr; // живёт на ioThread_, см. комментарий выше

    std::thread keepAliveThread_;
    std::mutex keepAliveMutex_;
    std::condition_variable keepAliveCv_;
    bool keepAliveStop_ = false;
};

} // namespace retom
