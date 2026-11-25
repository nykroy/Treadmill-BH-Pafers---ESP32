#include "treadmill_transport.h"

#include <Arduino.h>
#if MODO_ACTUAL == MODO_OPERACION_SATELITE
#include <NimBLEDevice.h>
#endif

namespace treadmill {
static Receiver onReceive = nullptr;

#if MODO_ACTUAL == MODO_OPERACION_SATELITE
static NimBLEClient* client = nullptr;
static NimBLERemoteCharacteristic* writer = nullptr;

void setBleConnection(NimBLEClient* connection, NimBLERemoteCharacteristic* characteristic) {
    client = connection;
    writer = characteristic;
}
#endif

bool begin(Receiver receiver) {
    if (!receiver) return false;
    onReceive = receiver;
#if MODO_ACTUAL == MODO_OPERACION_INTEGRADO
    Serial1.begin(TREADMILL_UART_BAUD, SERIAL_8N1, TREADMILL_UART_RX_PIN, TREADMILL_UART_TX_PIN);
    return static_cast<bool>(Serial1);
#else
    return true;
#endif
}

void receive(const uint8_t* bytes, size_t length) {
    if (onReceive && bytes && length) onReceive(bytes, length);
}

void poll() {
#if MODO_ACTUAL == MODO_OPERACION_INTEGRADO
    uint8_t bytes[64];
    size_t count = 0;
    while (count < sizeof(bytes) && Serial1.available() > 0) {
        int value = Serial1.read();
        if (value < 0) break;
        bytes[count++] = static_cast<uint8_t>(value);
    }
    if (count) receive(bytes, count);
#endif
}

bool write(const pafers::Frame& frame) {
    if (!frame.size) return false;
#if MODO_ACTUAL == MODO_OPERACION_SATELITE
    return client && client->isConnected() && writer &&
           writer->writeValue(frame.bytes, frame.size, writer->canWrite());
#else
    return Serial1.availableForWrite() >= static_cast<int>(frame.size) &&
           Serial1.write(frame.bytes, frame.size) == frame.size;
#endif
}

const char* txLabel() {
#if MODO_ACTUAL == MODO_OPERACION_SATELITE
    return "TX -> BLE";
#else
    return "TX -> UART";
#endif
}

const char* rxLabel() {
#if MODO_ACTUAL == MODO_OPERACION_SATELITE
    return "RX <- BLE";
#else
    return "RX <- UART";
#endif
}
}
