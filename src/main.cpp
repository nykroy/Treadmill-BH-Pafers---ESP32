#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <atomic>
#include "pafers_protocol.h"
#include "web_ui.h"

#include "wifi_config.h"
const IPAddress LOCAL_IP(192, 168, 218, 61);
const IPAddress GATEWAY(192, 168, 218, 1);
const IPAddress SUBNET(255, 255, 255, 0);
const char* TARGET_NAME = "PAFERS_C7BE99";

WebServer server(80);
SemaphoreHandle_t stateMutex;
QueueHandle_t commands;
enum class BleState { Disconnected, Scanning, Connecting, Connected };
enum class Operation { Scan, Connect, Disconnect, Start, Stop, Speed, Incline, Raw };
struct Request {
    Operation operation;
    char address[18]{};
    uint8_t addressType = 0;
    int value = 0;
    bool relative = false;
    pafers::Frame frame;
};
struct Device {
    char name[64]{};
    char address[18]{};
    int rssi = 0;
    uint8_t type = 0;
    bool target = false;
};
struct LogEntry { uint32_t id = 0; uint32_t ms = 0; char text[224]{}; };
struct SharedState {
    BleState state = BleState::Disconnected;
    char error[160]{};
    char address[18]{};
    bool gatt = false;
    int rssi = 0;
    bool rssiValid = false;
    Device devices[24];
    size_t deviceCount = 0;
    bool scanTruncated = false;
    uint32_t scanGeneration = 0;
    LogEntry logs[64];
    uint32_t logId = 0;
    bool telemetry = false;
    uint32_t telemetryAt = 0;
    uint16_t speedHundredths = 0;
    uint8_t incline = 0;
    int targetSpeed = 10;
    int targetIncline = 0;
    char motion[32] = "Desconocido";
    double distanceKm = 0;
    double elapsedSeconds = 0;
} shared;

class Guard {
public:
    Guard() { xSemaphoreTake(stateMutex, portMAX_DELAY); }
    ~Guard() { xSemaphoreGive(stateMutex); }
};

std::atomic<uint32_t> usbLogsDropped{0};
void logMessage(const char* text) {
    uint32_t now = millis();
    {
        Guard guard;
        LogEntry& entry = shared.logs[shared.logId % 64];
        entry.id = ++shared.logId;
        entry.ms = now;
        snprintf(entry.text, sizeof(entry.text), "%s", text);
    }
    char line[256];
    int length = snprintf(line, sizeof(line), "[%lu] %s\n", static_cast<unsigned long>(now), text);
    if (length > 0) {
        size_t size = min(static_cast<size_t>(length), sizeof(line) - 1);
        if (!Serial || Serial.write(reinterpret_cast<const uint8_t*>(line), size) != size)
            usbLogsDropped.fetch_add(1);
    }
}
void errorMessage(const char* text) {
    { Guard guard; snprintf(shared.error, sizeof(shared.error), "%s", text); }
    logMessage(text);
}
void logHex(const char* direction, const uint8_t* bytes, size_t length) {
    // Split long notifications so the bounded ring never truncates packet data.
    for (size_t base = 0; base < length; base += 60) {
        char line[224];
        size_t used = snprintf(line, sizeof(line), "%s%s ", direction, base ? " (cont.)" : "");
        for (size_t i = base; i < length && i < base + 60; ++i)
            used += snprintf(line + used, sizeof(line) - used, "%02X ", bytes[i]);
        logMessage(line);
    }
}

NimBLEClient* client = nullptr;
NimBLERemoteCharacteristic* writer = nullptr;
std::atomic<bool> linkLost{false};
std::atomic<bool> cancelConnection{false};
std::atomic<bool> bleReady{false};
std::atomic<bool> expectedDisconnect{false};
std::atomic<int> scanReason{-1};
pafers::StreamDecoder decoder;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onScanEnd(const NimBLEScanResults&, int reason) override { scanReason.store(reason); }
} scanCallbacks;

class ClientCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*, int reason) override {
        if (!expectedDisconnect.exchange(false)) linkLost.store(true);
        char message[80];
        snprintf(message, sizeof(message), "BLE desconectado, motivo %d", reason);
        logMessage(message);
    }
} clientCallbacks;

void notification(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
    logHex("RX <- BLE", data, length);
    bool valid = true;
    for (size_t i = 0; i < length; ++i) {
        valid = decoder.push(data[i], [](const pafers::Decoded& packet) {
            Guard guard;
            if (packet.kind == pafers::Kind::Telemetry) {
                shared.telemetry = true;
                shared.telemetryAt = millis();
                shared.speedHundredths = packet.speedHundredths;
                shared.incline = packet.incline;
                // Some BH consoles report exactly 1 km/h even when stopped.
                snprintf(shared.motion, sizeof(shared.motion), "%s",
                    packet.speedHundredths == 100 ? "Ambiguo (1 km/h)" :
                    packet.speedHundredths ? "Movimiento (inferido)" : "Parada (inferida)");
            } else if (packet.kind == pafers::Kind::StartButton ||
                       packet.kind == pafers::Kind::StopButton) {
                snprintf(shared.motion, sizeof(shared.motion), "%s",
                    packet.kind == pafers::Kind::StartButton ? "START en consola" : "STOP en consola");
            }
        }) && valid;
    }
    if (!valid) logMessage("RX: bytes fuera de trama o longitud invalida");
}

bool sendFrame(const pafers::Frame& frame) {
    if (!frame.size || !client || !client->isConnected() || !writer) {
        errorMessage("TX rechazado: BLE no disponible o trama vacia");
        return false;
    }
    if (!writer->writeValue(frame.bytes, frame.size, writer->canWrite())) {
        errorMessage("Error GATT al escribir");
        return false;
    }
    logHex("TX -> BLE", frame.bytes, frame.size);
    return true;
}

bool closeConnection() {
    if (client && client->isConnected()) {
        expectedDisconnect.store(true);
        if (!client->disconnect()) {
            expectedDisconnect.store(false);
            errorMessage("No se pudo solicitar la desconexion GATT");
            return false;
        }
    }
    writer = nullptr;
    Guard guard;
    shared.state = BleState::Disconnected;
    shared.gatt = false;
    shared.rssiValid = false;
    shared.telemetry = false;
    shared.address[0] = 0;
    snprintf(shared.motion, sizeof(shared.motion), "Desconocido");
    return true;
}

void scanDevices() {
    logMessage("Escaneo BLE activo: 5 segundos");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCallbacks);
    scan->setActiveScan(true);
    scan->setMaxResults(24);
    scanReason.store(-1);
    if (!scan->start(5000, false)) {
        errorMessage("No se pudo iniciar el escaneo BLE");
        Guard guard;
        shared.state = BleState::Disconnected;
        return;
    }
    uint32_t started = millis();
    while (scanReason.load() < 0 && millis() - started < 6500) vTaskDelay(pdMS_TO_TICKS(20));
    if (scanReason.load() != 0) {
        scan->stop();
        errorMessage("Escaneo BLE interrumpido o fuera de tiempo");
        Guard guard;
        shared.state = BleState::Disconnected;
        return;
    }
    NimBLEScanResults results = scan->getResults();
    {
        Guard guard;
        shared.deviceCount = 0;
        shared.scanTruncated = results.getCount() >= 24;
        for (int i = 0; i < results.getCount() && shared.deviceCount < 24; ++i) {
            const NimBLEAdvertisedDevice* found = results.getDevice(i);
            Device& device = shared.devices[shared.deviceCount++];
            snprintf(device.name, sizeof(device.name), "%s", found->getName().c_str());
            snprintf(device.address, sizeof(device.address), "%s", found->getAddress().toString().c_str());
            device.type = found->getAddress().getType();
            device.rssi = found->getRSSI();
            device.target = found->getName() == TARGET_NAME;
        }
        ++shared.scanGeneration;
        shared.state = BleState::Disconnected;
    }
    scan->clearResults();
    logMessage("Escaneo BLE finalizado");
}

bool connectDevice(const Request& request) {
    if (cancelConnection.load()) return false;
    linkLost.store(false);
    decoder.reset();
    if (!client) {
        client = NimBLEDevice::createClient();
        if (!client) { errorMessage("Sin memoria para cliente BLE"); return false; }
        client->setClientCallbacks(&clientCallbacks, false);
        client->setConnectTimeout(8000);
    }
    if (!client->connect(NimBLEAddress(std::string(request.address), request.addressType))) {
        errorMessage("No se pudo conectar al dispositivo seleccionado");
        return false;
    }
    if (cancelConnection.load()) return false;
    NimBLERemoteService* service = client->getService(pafers::ServiceUuid);
    if (!service) { errorMessage("Servicio Pafers no encontrado"); return false; }
    writer = service->getCharacteristic(pafers::WriteUuid);
    NimBLERemoteCharacteristic* notify = service->getCharacteristic(pafers::NotifyUuid);
    if (!writer || (!writer->canWrite() && !writer->canWriteNoResponse()) ||
        !notify || !notify->canNotify()) {
        errorMessage("GATT Pafers Write/Notify no compatible");
        return false;
    }
    if (!notify->subscribe(true, notification)) {
        errorMessage("Fallo al suscribirse a Notify");
        return false;
    }
    { Guard guard; shared.gatt = true; }
    for (size_t i = 0; i < pafers::HandshakeSteps; ++i) {
        if (cancelConnection.load() || !sendFrame(pafers::handshake(i))) return false;
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    if (!client->isConnected() || cancelConnection.load()) return false;
    {
        Guard guard;
        shared.state = BleState::Connected;
        shared.targetSpeed = 10;
        shared.targetIncline = 0;
        shared.distanceKm = 0;
        shared.elapsedSeconds = 0;
        snprintf(shared.address, sizeof(shared.address), "%s", request.address);
    }
    logMessage("BLE conectado; inicializacion transmitida (sin confirmacion de consola)");
    return true;
}

void bleWorker(void*) {
    bool initAttempted = false;
    uint32_t lastKeepAlive = millis(), lastRssi = 0, lastEstimate = millis();
    for (;;) {
        if (!bleReady.load()) {
            if (!initAttempted && WiFi.status() == WL_CONNECTED) {
                initAttempted = true;
                logMessage("Wi-Fi asociado; inicializando el controlador BLE");
                if (NimBLEDevice::init("BH-Web-Bridge")) bleReady.store(true);
                else errorMessage("No se pudo inicializar NimBLE");
            }
            if (!bleReady.load()) {
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }
        }
        if (linkLost.exchange(false)) {
            errorMessage("Enlace BLE cerrado; reconexion solo manual");
            closeConnection();
        }
        Request request{};
        if (xQueueReceive(commands, &request, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (request.operation == Operation::Scan) scanDevices();
            else if (request.operation == Operation::Connect) {
                if (!connectDevice(request)) closeConnection();
                lastKeepAlive = millis();
                lastEstimate = millis();
            } else if (request.operation == Operation::Disconnect) {
                if (closeConnection()) logMessage("Desconexion manual solicitada");
            } else {
                bool connected;
                { Guard guard; connected = shared.state == BleState::Connected; }
                if (!connected || cancelConnection.load()) {
                    errorMessage("Orden descartada: enlace ya no disponible");
                    continue;
                }
                bool sent = false;
                if (request.operation == Operation::Start) sent = sendFrame(pafers::start());
                if (request.operation == Operation::Stop) sent = sendFrame(pafers::stop());
                if (request.operation == Operation::Raw) sent = sendFrame(request.frame);
                if (request.operation == Operation::Speed || request.operation == Operation::Incline) {
                    int speed, incline;
                    { Guard guard; speed = shared.targetSpeed; incline = shared.targetIncline; }
                    if (request.operation == Operation::Speed)
                        speed = request.relative ? speed + request.value : request.value;
                    else incline = request.relative ? incline + request.value : request.value;
                    if (!pafers::validSpeedTarget(speed) ||
                        (request.operation == Operation::Incline && !pafers::validInclineTarget(incline)) ||
                        (request.operation == Operation::Speed && (incline < 0 || incline > pafers::MaxIncline))) {
                        errorMessage("Ajuste rechazado: limite de velocidad o inclinacion");
                        continue;
                    }
                    sent = sendFrame(pafers::speed(speed));
                    if (sent) {
                        vTaskDelay(pdMS_TO_TICKS(400));
                        sent = !cancelConnection.load() && sendFrame(pafers::incline(incline));
                    }
                    if (sent) {
                        Guard guard;
                        shared.targetSpeed = speed;
                        shared.targetIncline = incline;
                    }
                }
                if (!sent) errorMessage("Orden BLE no completada");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        bool connected;
        { Guard guard; connected = shared.state == BleState::Connected; }
        uint32_t now = millis();
        if (connected && !cancelConnection.load()) {
            if (now - lastKeepAlive >= 500) {
                if (!sendFrame(pafers::keepAlive())) closeConnection();
                lastKeepAlive = now;
            }
            if (now - lastRssi >= 2000 && client->isConnected()) {
                int rssi = client->getRssi();
                Guard guard;
                shared.rssi = rssi;
                shared.rssiValid = rssi != 0;
                lastRssi = now;
            }
            Guard guard;
            if (shared.telemetry && now - shared.telemetryAt < 3000 &&
                shared.speedHundredths > 0 && shared.speedHundredths != 100) {
                double seconds = (now - lastEstimate) / 1000.0;
                shared.elapsedSeconds += seconds;
                shared.distanceKm += shared.speedHundredths / 100.0 * seconds / 3600.0;
            }
        }
        lastEstimate = now;
    }
}

void reply(int code, const char* message) {
    JsonDocument document;
    document["message"] = message;
    String body;
    serializeJson(document, body);
    server.send(code, "application/json", body);
}
bool readBody(JsonDocument& document) {
    if (server.arg("plain").length() > 256) { reply(413, "Cuerpo demasiado grande"); return false; }
    if (deserializeJson(document, server.arg("plain"))) {
        reply(400, "JSON invalido"); return false;
    }
    return true;
}
bool validAddress(const char* address) {
    if (strlen(address) != 17) return false;
    for (size_t i = 0; i < 17; ++i) {
        if (i % 3 == 2 ? address[i] != ':' : pafers::nibble(address[i]) < 0) return false;
    }
    return true;
}
void submit(Operation operation) {
    if (!bleReady.load()) { reply(503, "Backend BLE todavia no disponible"); return; }
    Request request{};
    request.operation = operation;
    JsonDocument document;
    if (operation == Operation::Connect || operation == Operation::Raw ||
        operation == Operation::Speed || operation == Operation::Incline) {
        if (!readBody(document)) return;
    }
    if (operation == Operation::Connect) {
        const char* address = document["address"] | "";
        if (*address && !validAddress(address)) { reply(400, "MAC invalida"); return; }
        if (*address) {
            if (!document["type"].is<int>() || document["type"].as<int>() < 0 ||
                document["type"].as<int>() > 1) { reply(400, "Tipo de direccion invalido"); return; }
            snprintf(request.address, sizeof(request.address), "%s", address);
            request.addressType = document["type"].as<uint8_t>();
        } else {
            Guard guard;
            for (size_t i = 0; i < shared.deviceCount; ++i) {
                if (shared.devices[i].target) {
                    snprintf(request.address, sizeof(request.address), "%s", shared.devices[i].address);
                    request.addressType = shared.devices[i].type;
                    break;
                }
            }
        }
        if (!*request.address) { reply(409, "Escanee y seleccione PAFERS_C7BE99 primero"); return; }
    }
    if (operation == Operation::Raw &&
        !pafers::parseHex(document["hex"] | "", request.frame)) {
        reply(400, "HEX invalido: 1 a 20 bytes, pares hexadecimales sin prefijo 0x"); return;
    }
    if (operation == Operation::Speed || operation == Operation::Incline) {
        request.relative = !document["delta"].isNull();
        if (request.relative) {
            if (!document["delta"].is<int>() || !document["value"].isNull()) {
                reply(400, "delta debe ser entero y no puede combinarse con value"); return;
            }
            request.value = document["delta"].as<int>();
            bool validDelta = request.value == -1 || request.value == 1;
            if (operation == Operation::Speed)
                validDelta = validDelta || request.value == -5 || request.value == 5;
            if (!validDelta) {
                reply(400, operation == Operation::Speed
                    ? "delta de velocidad permitido: -5, -1, 1 o 5 decimas"
                    : "delta de inclinacion permitido: -1 o 1");
                return;
            }
        } else {
            if (!document["value"].is<int>()) { reply(400, "value debe ser entero"); return; }
            request.value = document["value"].as<int>();
            if (operation == Operation::Speed ? !pafers::validSpeedTarget(request.value)
                                              : !pafers::validInclineTarget(request.value)) {
                reply(400, "Valor fuera de limites"); return;
            }
        }
    }
    int code = 202;
    const char* message = "Operacion encolada; consultar /api/status";
    {
        Guard guard;
        BleState before = shared.state;
        bool management = operation == Operation::Scan || operation == Operation::Connect;
        if ((management && before != BleState::Disconnected) ||
            (!management && operation != Operation::Disconnect && before != BleState::Connected)) {
            code = 409; message = "Operacion incompatible con el estado BLE";
        } else {
            if (operation == Operation::Scan) shared.state = BleState::Scanning;
            if (operation == Operation::Connect) shared.state = BleState::Connecting;
            if (operation == Operation::Disconnect || operation == Operation::Stop) {
                xQueueReset(commands);
            }
            if (xQueueSend(commands, &request, 0) != pdTRUE) {
                shared.state = before;
                code = 503; message = "Cola BLE llena";
            } else {
                shared.error[0] = 0;
                if (operation == Operation::Connect) cancelConnection.store(false);
                if (operation == Operation::Disconnect) cancelConnection.store(true);
            }
        }
    }
    reply(code, message);
}

void status() {
    JsonDocument document;
    {
        Guard guard;
        const char* names[] = {"disconnected", "scanning", "connecting", "connected"};
        document["state"] = names[static_cast<int>(shared.state)];
        document["error"] = shared.error;
        document["usbLogsDropped"] = usbLogsDropped.load();
        document["address"] = shared.address;
        if (shared.rssiValid) document["rssi"] = shared.rssi;
        else document["rssi"] = nullptr;
        document["serviceUuid"] = shared.gatt ? pafers::ServiceUuid : "";
        document["writeUuid"] = shared.gatt ? pafers::WriteUuid : "";
        document["notifyUuid"] = shared.gatt ? pafers::NotifyUuid : "";
        document["scanGeneration"] = shared.scanGeneration;
        document["scanTruncated"] = shared.scanTruncated;
        document["targetSpeedTenths"] = shared.targetSpeed;
        document["targetIncline"] = shared.targetIncline;
        document["telemetryFresh"] = shared.telemetry && millis() - shared.telemetryAt < 3000;
        document["speed"] = shared.speedHundredths / 100.0;
        document["incline"] = shared.incline;
        document["motion"] = shared.motion;
        document["distanceKm"] = shared.distanceKm;
        document["elapsedSeconds"] = shared.elapsedSeconds;
        JsonArray devices = document["devices"].to<JsonArray>();
        for (size_t i = 0; i < shared.deviceCount; ++i) {
            JsonObject device = devices.add<JsonObject>();
            device["name"] = shared.devices[i].name;
            device["address"] = shared.devices[i].address;
            device["rssi"] = shared.devices[i].rssi;
            device["type"] = shared.devices[i].type;
            device["target"] = shared.devices[i].target;
        }
    }
    String body;
    serializeJson(document, body);
    server.send(200, "application/json", body);
}

void logs() {
    uint32_t since = strtoul(server.arg("since").c_str(), nullptr, 10);
    JsonDocument document;
    JsonArray entries = document["entries"].to<JsonArray>();
    {
        Guard guard;
        uint32_t first = shared.logId > 64 ? shared.logId - 63 : 1;
        document["lost"] = since && since < first - 1;
        document["latest"] = shared.logId;
        for (uint32_t id = first; id <= shared.logId; ++id) {
            if (id <= since) continue;
            const LogEntry& entry = shared.logs[(id - 1) % 64];
            JsonObject item = entries.add<JsonObject>();
            item["id"] = entry.id;
            item["ms"] = entry.ms;
            item["text"] = entry.text;
        }
    }
    String body;
    serializeJson(document, body);
    server.send(200, "application/json", body);
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    stateMutex = xSemaphoreCreateMutex();
    commands = xQueueCreate(8, sizeof(Request));
    if (!stateMutex || !commands) {
        Serial.println("ERROR FATAL: sin memoria para sincronizacion BLE");
        while (true) delay(1000);
    }
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("cinta");
    WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            logMessage("Wi-Fi asociado al punto de acceso");
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            IPAddress ip = WiFi.localIP();
            char message[96];
            snprintf(message, sizeof(message), "Wi-Fi listo; IP=%u.%u.%u.%u",
                ip[0], ip[1], ip[2], ip[3]);
            logMessage(message);
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            const uint8_t reason = info.wifi_sta_disconnected.reason;
            char message[112];
            snprintf(message, sizeof(message), "Wi-Fi desconectado; motivo=%u (%s)",
                static_cast<unsigned>(reason),
                WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
            logMessage(message);
        }
    });
    if (!WiFi.config(LOCAL_IP, GATEWAY, SUBNET, GATEWAY)) logMessage("ERROR: configuracion IP fija rechazada");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", WEB_UI); });
    server.on("/api/status", HTTP_GET, status);
    server.on("/api/logs", HTTP_GET, logs);
    server.on("/api/ble/scan", HTTP_POST, [] { submit(Operation::Scan); });
    server.on("/api/ble/connect", HTTP_POST, [] { submit(Operation::Connect); });
    server.on("/api/ble/disconnect", HTTP_POST, [] { submit(Operation::Disconnect); });
    server.on("/api/ble/raw", HTTP_POST, [] { submit(Operation::Raw); });
    server.on("/api/control/start", HTTP_POST, [] { submit(Operation::Start); });
    server.on("/api/control/stop", HTTP_POST, [] { submit(Operation::Stop); });
    server.on("/api/control/speed", HTTP_POST, [] { submit(Operation::Speed); });
    server.on("/api/control/incline", HTTP_POST, [] { submit(Operation::Incline); });
    server.onNotFound([] { reply(404, "Ruta no encontrada"); });
    server.begin();
    if (xTaskCreate(bleWorker, "ble-worker", 8192, nullptr, 1, nullptr) != pdPASS)
        errorMessage("ERROR FATAL: no se pudo crear la tarea BLE");
    logMessage("HTTP iniciado; esperando Wi-Fi, sin conexion BLE automatica");
}

void loop() {
    server.handleClient();
    static bool wifiUp = false, mdnsUp = false;
    static uint32_t lastRetry = 0;
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !wifiUp) {
        logMessage("Wi-Fi conectado: http://192.168.218.61 / http://cinta.local");
        lastRetry = millis() - 10000;
    }
    if (connected && !mdnsUp && millis() - lastRetry >= 10000) {
        lastRetry = millis();
        mdnsUp = MDNS.begin("cinta");
        if (mdnsUp) MDNS.addService("http", "tcp", 80);
        else errorMessage("No se pudo iniciar mDNS; reintentando");
    }
    if (!connected && wifiUp) {
        errorMessage("Wi-Fi desconectado");
        if (mdnsUp) MDNS.end();
        mdnsUp = false;
    }
    if (!connected && millis() - lastRetry >= 15000) {
        lastRetry = millis();
        char message[96];
        snprintf(message, sizeof(message), "Sin Wi-Fi; wl_status=%d; esperando asociacion",
            static_cast<int>(WiFi.status()));
        logMessage(message);
        if (WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_NO_SSID_AVAIL ||
            WiFi.status() == WL_CONNECT_FAILED) {
            WiFi.reconnect();
        }
    }
    wifiUp = connected;
    delay(2);
}
