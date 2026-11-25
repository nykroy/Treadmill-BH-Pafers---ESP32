#include "ftms_server.h"

#if MODO_ACTUAL == MODO_OPERACION_INTEGRADO
#include <NimBLEDevice.h>
#include <atomic>
#include <math.h>

namespace ftms {
static constexpr uint16_t NoHandle = 0xffff;
static std::atomic<uint16_t> owner{NoHandle};
static std::atomic<uint16_t> subscribed{NoHandle};
static std::atomic<uint16_t> dataSubscriber{NoHandle};
static std::atomic<uint16_t> connectedHandle{NoHandle};
static std::atomic<uint16_t> pendingHandle{NoHandle};
static std::atomic<uint8_t> pendingOpcode{0};
static std::atomic<uint32_t> nextToken{0};
static std::atomic<uint32_t> pendingToken{0};
static std::atomic<bool> busy{false};
static std::atomic<unsigned> clientCount{0};
static Submit submitCommand = nullptr;
static Log logMessage = nullptr;
static NimBLECharacteristic* control = nullptr;
static NimBLECharacteristic* dataCharacteristic = nullptr;

static void respond(uint16_t handle, uint8_t opcode, uint8_t result) {
    const uint8_t response[] = {0x80, opcode, result};
    if (!control || subscribed.load() != handle || !control->indicate(response, sizeof(response), handle)) {
        busy.store(false);
        if (logMessage) logMessage("FTMS: no se pudo enviar la indicacion de respuesta");
    }
}

static bool hasControl(uint16_t handle) {
    return handle != NoHandle && owner.load() == handle && subscribed.load() == handle;
}

bool authorized(uint16_t handle, uint32_t token) {
    return token && pendingToken.load() == token && pendingHandle.load() == handle && hasControl(handle);
}

void complete(uint16_t handle, uint32_t token, uint8_t opcode, bool success) {
    if (!token || pendingToken.load() != token || pendingHandle.load() != handle || pendingOpcode.load() != opcode) return;
    pendingHandle.store(NoHandle);
    pendingToken.store(0);
    if (!hasControl(handle)) {
        busy.store(false);
        if (logMessage) logMessage("FTMS: cliente desconectado antes de finalizar la orden");
        return;
    }
    respond(handle, opcode, success ? 1 : 4);
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
        uint16_t expected = NoHandle;
        if (!connectedHandle.compare_exchange_strong(expected, info.getConnHandle())) {
            server->disconnect(info);
            return;
        }
        clientCount.store(1);
        if (logMessage) logMessage("FTMS: cliente conectado");
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) override {
        const uint16_t handle = info.getConnHandle();
        if (connectedHandle.load() != handle) return;
        connectedHandle.store(NoHandle);
        if (owner.load() == handle) owner.store(NoHandle);
        if (subscribed.load() == handle) subscribed.store(NoHandle);
        if (dataSubscriber.load() == handle) dataSubscriber.store(NoHandle);
        if (pendingHandle.load() == handle) {
            pendingHandle.store(NoHandle);
            pendingToken.store(0);
        }
        busy.store(false);
        clientCount.store(0);
        if (logMessage) logMessage("FTMS: cliente desconectado");
    }
} serverCallbacks;

class ControlCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& info, uint16_t value) override {
        const uint16_t handle = info.getConnHandle();
        if (value & 2) subscribed.store(handle);
        else if (subscribed.load() == handle) {
            subscribed.store(NoHandle);
            if (owner.load() == handle) owner.store(NoHandle);
        }
    }
    void onStatus(NimBLECharacteristic*, NimBLEConnInfo& info, int code) override {
        if (subscribed.load() != info.getConnHandle()) return;
        if (code == 0) return;
        busy.store(false);
        if (code != BLE_HS_EDONE && logMessage) logMessage("FTMS: indicacion no confirmada");
    }
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override {
        const uint16_t handle = info.getConnHandle();
        if (subscribed.load() != handle) {
            if (logMessage) logMessage("FTMS: habilite las indicaciones antes de enviar ordenes");
            return;
        }
        if (busy.exchange(true)) {
            if (logMessage) logMessage("FTMS: orden rechazada; respuesta anterior pendiente");
            return;
        }
        const NimBLEAttValue value = characteristic->getValue();
        Command command = decodeControl(value.data(), value.size());
        if (command.action == Action::Invalid) { respond(handle, command.opcode, 3); return; }
        if (command.action == Action::Unsupported) { respond(handle, command.opcode, 2); return; }
        if (command.action == Action::RequestControl) {
            if (owner.load() != NoHandle && owner.load() != handle) respond(handle, command.opcode, 5);
            else { owner.store(handle); respond(handle, command.opcode, 1); }
            return;
        }
        if (!hasControl(handle)) { respond(handle, command.opcode, 5); return; }
        uint32_t token = ++nextToken;
        if (!token) token = ++nextToken;
        pendingHandle.store(handle);
        pendingOpcode.store(command.opcode);
        pendingToken.store(token);
        if (!submitCommand || !submitCommand(command, handle, token)) {
            pendingHandle.store(NoHandle);
            pendingToken.store(0);
            respond(handle, command.opcode, 4);
        }
    }
} controlCallbacks;

class DataCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& info, uint16_t value) override {
        if (value & 1) dataSubscriber.store(info.getConnHandle());
        else if (dataSubscriber.load() == info.getConnHandle()) dataSubscriber.store(NoHandle);
    }
} dataCallbacks;

bool begin(Submit submit, Log log) {
    if (!submit || !log) return false;
    submitCommand = submit;
    logMessage = log;
    NimBLEServer* server = NimBLEDevice::createServer();
    if (!server) return false;
    server->setCallbacks(&serverCallbacks, false);
    server->advertiseOnDisconnect(true);
    NimBLEService* service = server->createService("1826");
    if (!service) return false;
    auto* feature = service->createCharacteristic("2ACC", NIMBLE_PROPERTY::READ);
    auto* speedRange = service->createCharacteristic("2AD4", NIMBLE_PROPERTY::READ);
    auto* inclineRange = service->createCharacteristic("2AD5", NIMBLE_PROPERTY::READ);
    auto* status = service->createCharacteristic("2ADA", NIMBLE_PROPERTY::NOTIFY);
    dataCharacteristic = service->createCharacteristic("2ACD", NIMBLE_PROPERTY::NOTIFY);
    control = service->createCharacteristic("2AD9", NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    if (!feature || !speedRange || !inclineRange || !status || !dataCharacteristic || !control) return false;
    const uint8_t features[] = {0x0c,0,0,0, 0x03,0,0,0};
    const uint8_t speeds[] = {0,0, 0x40,0x06, 10,0};
    const uint8_t inclines[] = {10,0, 150,0, 10,0};
    feature->setValue(features, sizeof(features));
    speedRange->setValue(speeds, sizeof(speeds));
    inclineRange->setValue(inclines, sizeof(inclines));
    control->setCallbacks(&controlCallbacks);
    dataCharacteristic->setCallbacks(&dataCallbacks);
    if (!server->start()) return false;
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    return advertising && advertising->setName("BH-iBooster-FTMS") &&
           advertising->addServiceUUID("1826") && advertising->start();
}

unsigned clients() { return clientCount.load(); }

void publish(uint16_t speedHundredths, uint8_t incline, uint32_t distanceMeters) {
    const uint16_t handle = dataSubscriber.load();
    if (!dataCharacteristic || handle == NoHandle) return;
    uint8_t data[11];
    const auto degrees = static_cast<uint16_t>(lround(atan(incline / 100.0) * (1800.0 / 3.141592653589793)));
    treadmillData(data, speedHundredths, incline, degrees, distanceMeters);
    if (!dataCharacteristic->notify(data, sizeof(data), handle) && logMessage)
        logMessage("FTMS: no se pudo enviar telemetria");
}
}
#endif
