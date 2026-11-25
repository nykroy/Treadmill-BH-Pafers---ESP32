#pragma once

#include "config.h"
#include "pafers_protocol.h"

#if MODO_ACTUAL == MODO_OPERACION_SATELITE
class NimBLEClient;
class NimBLERemoteCharacteristic;
#endif

namespace treadmill {
using Receiver = void (*)(const uint8_t*, size_t);
bool begin(Receiver receiver);
void poll();
bool write(const pafers::Frame& frame);
void receive(const uint8_t* bytes, size_t length);
const char* txLabel();
const char* rxLabel();
#if MODO_ACTUAL == MODO_OPERACION_SATELITE
void setBleConnection(NimBLEClient* client, NimBLERemoteCharacteristic* writer);
#endif
}
