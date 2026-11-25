#pragma once

#include <stddef.h>
#include <stdint.h>
#include "pafers_protocol.h"

namespace ftms {
enum class Action { RequestControl, Start, Stop, Speed, Incline, Invalid, Unsupported };
struct Command {
    Action action = Action::Invalid;
    uint8_t opcode = 0;
    int value = 0;
};

inline Command decodeControl(const uint8_t* data, size_t size) {
    Command command;
    if (!data || !size) return command;
    command.opcode = data[0];
    switch (command.opcode) {
        case 0x00:
            if (size == 1) command.action = Action::RequestControl;
            break;
        case 0x07:
            if (size == 1) command.action = Action::Start;
            break;
        case 0x08:
            if (size == 2 && data[1] == 1) command.action = Action::Stop;
            else if (size == 2 && data[1] == 2) command.action = Action::Unsupported;
            break;
        case 0x02:
            if (size == 3) {
                unsigned speed = unsigned(data[1]) | (unsigned(data[2]) << 8);
                if (speed % 10 == 0 && pafers::validSpeedTarget(speed / 10)) {
                    command.action = Action::Speed;
                    command.value = speed / 10;
                }
            }
            break;
        case 0x03:
            if (size == 3) {
                int value = int(data[1]) | (int(data[2]) << 8);
                if (value >= 0x8000) value -= 0x10000;
                if (value % 10 == 0 && pafers::validInclineTarget(value / 10)) {
                    command.action = Action::Incline;
                    command.value = value / 10;
                }
            }
            break;
        default:
            command.action = Action::Unsupported;
    }
    return command;
}

inline void treadmillData(uint8_t (&out)[11], uint16_t speedHundredths,
                          uint8_t incline, uint16_t rampTenths, uint32_t distanceMeters) {
    if (distanceMeters > 0xffffff) distanceMeters = 0xffffff;
    const uint16_t percentTenths = uint16_t(incline) * 10;
    out[0] = 0x0c; out[1] = 0;
    out[2] = speedHundredths & 0xff; out[3] = speedHundredths >> 8;
    out[4] = distanceMeters & 0xff; out[5] = (distanceMeters >> 8) & 0xff;
    out[6] = (distanceMeters >> 16) & 0xff;
    out[7] = percentTenths & 0xff; out[8] = percentTenths >> 8;
    out[9] = rampTenths & 0xff; out[10] = rampTenths >> 8;
}
}
