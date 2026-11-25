#pragma once

#include "config.h"

#if MODO_ACTUAL == MODO_OPERACION_INTEGRADO
#include "ftms_protocol.h"

namespace ftms {
using Submit = bool (*)(const Command&, uint16_t, uint32_t);
using Log = void (*)(const char*);
bool begin(Submit submit, Log log);
bool authorized(uint16_t handle, uint32_t token);
void complete(uint16_t handle, uint32_t token, uint8_t opcode, bool success);
unsigned clients();
void publish(uint16_t speedHundredths, uint8_t incline, uint32_t distanceMeters);
}
#endif
