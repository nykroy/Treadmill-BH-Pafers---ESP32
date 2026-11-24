#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pafers {
constexpr const char* ServiceUuid = "72d70001-501f-46f7-95f9-23846ee1aba3";
constexpr const char* WriteUuid = "72d70002-501f-46f7-95f9-23846ee1aba3";
constexpr const char* NotifyUuid = "72d70003-501f-46f7-95f9-23846ee1aba3";
constexpr uint16_t MaxSpeedTenths = 160;
constexpr uint8_t MinIncline = 1;
constexpr uint8_t MaxIncline = 15;

inline bool validSpeedTarget(int tenths) {
    return tenths >= 0 && tenths <= MaxSpeedTenths;
}
inline bool validInclineTarget(int value) {
    return value >= MinIncline && value <= MaxIncline;
}

// Pafers treadmill framing has a payload length, NOT a trailing checksum.
struct Frame {
    uint8_t bytes[20]{};
    size_t size = 0;
};

inline Frame command(uint8_t opcode, const uint8_t* payload, size_t length) {
    Frame frame;
    if (length > sizeof(frame.bytes) - 3) return frame;
    frame.bytes[0] = 0x55;
    frame.bytes[1] = opcode;
    frame.bytes[2] = static_cast<uint8_t>(length);
    for (size_t i = 0; i < length; ++i) frame.bytes[i + 3] = payload[i];
    frame.size = length + 3;
    return frame;
}

inline Frame single(uint8_t opcode, uint8_t value) {
    return command(opcode, &value, 1);
}
inline Frame start() { return single(0x0a, 1); }
inline Frame stop() { return single(0x0a, 2); }
inline Frame speed(uint16_t tenths) {
    if (tenths > MaxSpeedTenths) return {};
    // Fractional writes are experimental; RX expresses fractions in hundredths.
    const uint8_t payload[] = {static_cast<uint8_t>(tenths / 10),
                               static_cast<uint8_t>((tenths % 10) * 10)};
    return command(0x0f, payload, sizeof(payload));
}
inline Frame incline(uint8_t value) {
    return value <= MaxIncline ? single(0x11, value) : Frame{};
}
inline Frame keepAlive() {
    const uint8_t payload[] = {255,255,255,255,255,255,255,255,255,255};
    return command(0x0d, payload, sizeof(payload));
}
constexpr size_t HandshakeSteps = 9;
inline Frame handshake(size_t step) {
    switch (step) {
        case 0: return single(0xbb, 255);
        case 1: return single(0x0c, 255);
        case 2: return single(0x1f, 255);
        case 3: return stop();
        case 4: {
            const uint8_t profile[] = {0x39,0x01,0x32,0x19,0xa5,0x32};
            return command(0x01, profile, sizeof(profile));
        }
        case 5: return single(0x15, 0);
        case 6: return speed(10);
        case 7: return incline(0);
        case 8: return single(0x08, 1);
        default: return {};
    }
}

enum class Kind { Unknown, Telemetry, StartButton, StopButton };
struct Decoded {
    Kind kind = Kind::Unknown;
    uint16_t speedHundredths = 0;
    uint8_t incline = 0;
};
inline Decoded decode(const uint8_t* data, size_t size) {
    Decoded result;
    if (size < 3 || data[0] != 0x55 || size != size_t(data[2]) + 3) return result;
    if (size == 4 && data[1] == 0x09 && data[2] == 1) {
        if (data[3] == 1) result.kind = Kind::StartButton;
        if (data[3] == 0) result.kind = Kind::StopButton;
    } else if (size == 13 && data[10] < 100 && data[9] <= 22 && data[11] <= 15) {
        // The reference identifies telemetry by length, not by an opcode.
        result.kind = Kind::Telemetry;
        result.speedHundredths = data[9] * 100 + data[10];
        result.incline = data[11];
    }
    return result;
}

class StreamDecoder {
    uint8_t buffer_[64]{};
    size_t used_ = 0;
public:
    void reset() { used_ = 0; }
    template<typename Receiver>
    bool push(uint8_t byte, Receiver receive) {
        if (!used_ && byte != 0x55) return false;
        buffer_[used_++] = byte;
        if (used_ >= 3 && buffer_[2] > sizeof(buffer_) - 3) {
            reset();
            return false;
        }
        if (used_ >= 3 && used_ == size_t(buffer_[2]) + 3) {
            receive(decode(buffer_, used_));
            reset();
        }
        return true;
    }
};

inline int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
inline bool parseHex(const char* text, Frame& frame) {
    frame = {};
    int high = -1;
    for (; *text; ++text) {
        if (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') continue;
        int value = nibble(*text);
        if (value < 0) return false;
        if (high < 0) high = value;
        else {
            if (frame.size == sizeof(frame.bytes)) return false;
            frame.bytes[frame.size++] = static_cast<uint8_t>((high << 4) | value);
            high = -1;
        }
    }
    return high < 0 && frame.size > 0;
}
} // namespace pafers
