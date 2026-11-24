#include "../src/pafers_protocol.h"

static bool equal(const pafers::Frame& frame, const uint8_t* expected, size_t size) {
    if (frame.size != size) return false;
    for (size_t i = 0; i < size; ++i) if (frame.bytes[i] != expected[i]) return false;
    return true;
}

int runProtocolTests() {
    const uint8_t start[] = {0x55,0x0a,1,1};
    const uint8_t stop[] = {0x55,0x0a,1,2};
    const uint8_t fractional[] = {0x55,0x0f,2,4,10};
    const uint8_t incline[] = {0x55,0x11,1,15};
    if (!equal(pafers::start(), start, sizeof(start))) return 1;
    if (!equal(pafers::stop(), stop, sizeof(stop))) return 2;
    if (!equal(pafers::speed(41), fractional, sizeof(fractional))) return 3;
    if (!equal(pafers::incline(15), incline, sizeof(incline))) return 4;
    const uint8_t maxSpeed[] = {0x55,0x0f,2,16,0};
    if (!equal(pafers::speed(160), maxSpeed, sizeof(maxSpeed)) ||
        pafers::speed(161).size || pafers::incline(16).size) return 5;
    if (!pafers::validSpeedTarget(0) || !pafers::validSpeedTarget(160) ||
        pafers::validSpeedTarget(-1) || pafers::validSpeedTarget(161) ||
        !pafers::validInclineTarget(1) || !pafers::validInclineTarget(15) ||
        pafers::validInclineTarget(0) || pafers::validInclineTarget(16)) return 21;
    pafers::Frame raw;
    if (!pafers::parseHex("55 0a\t01\n02", raw) || !equal(raw, stop, sizeof(stop))) return 6;
    const char* invalid[] = {"", "5", "55 0x0a", "55 GG", "000000000000000000000000000000000000000000"};
    for (const char* text : invalid) if (pafers::parseHex(text, raw)) return 7;
    if (!pafers::parseHex("0000000000000000000000000000000000000000", raw) || raw.size != 20) return 8;
    for (size_t i = 0; i < pafers::HandshakeSteps; ++i) {
        auto frame = pafers::handshake(i);
        if (!frame.size || frame.size != size_t(frame.bytes[2]) + 3) return 9;
        if (equal(frame, start, sizeof(start))) return 10;
    }
    if (pafers::handshake(pafers::HandshakeSteps).size) return 11;
    uint8_t telemetry[] = {0x55,0x0d,10,0,0,0,0,0,0,4,10,3,0};
    auto packet = pafers::decode(telemetry, sizeof(telemetry));
    if (packet.kind != pafers::Kind::Telemetry || packet.speedHundredths != 410 || packet.incline != 3) return 12;
    telemetry[10] = 100;
    if (pafers::decode(telemetry, sizeof(telemetry)).kind != pafers::Kind::Unknown) return 13;
    telemetry[10] = 10;
    if (pafers::decode(telemetry, 12).kind != pafers::Kind::Unknown) return 14;
    const uint8_t startEvent[] = {0x55,9,1,1}, stopEvent[] = {0x55,9,1,0};
    if (pafers::decode(startEvent, 4).kind != pafers::Kind::StartButton ||
        pafers::decode(stopEvent, 4).kind != pafers::Kind::StopButton) return 15;
    pafers::StreamDecoder stream;
    int received = 0;
    auto accept = [&](const pafers::Decoded& result) {
        if (result.kind != pafers::Kind::Unknown) ++received;
    };
    if (stream.push(0, accept)) return 16;
    for (size_t i = 0; i < 6; ++i) stream.push(telemetry[i], accept);
    if (received) return 17;
    for (size_t i = 6; i < sizeof(telemetry); ++i) stream.push(telemetry[i], accept);
    for (uint8_t byte : startEvent) stream.push(byte, accept);
    for (uint8_t byte : stopEvent) stream.push(byte, accept);
    if (received != 3) return 18;
    stream.push(0x55, accept); stream.push(0, accept);
    if (stream.push(255, accept)) return 19;
    for (uint8_t byte : startEvent) stream.push(byte, accept);
    if (received != 4) return 20;
    return 0;
}

#ifdef PAFERS_FREESTANDING_TEST
extern "C" void* memset(void* dest, int value, size_t length) {
    auto* out = static_cast<volatile uint8_t*>(dest);
    for (size_t i = 0; i < length; ++i) out[i] = static_cast<uint8_t>(value);
    return dest;
}
extern "C" void* memcpy(void* dest, const void* src, size_t length) {
    auto* out = static_cast<uint8_t*>(dest);
    const auto* in = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < length; ++i) out[i] = in[i];
    return dest;
}
extern "C" int testEntry() { return runProtocolTests(); }
#else
int main() { return runProtocolTests(); }
#endif
