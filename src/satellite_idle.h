#pragma once

#include <stdint.h>

class SatelliteIdle {
    static constexpr uint32_t TimeoutMs = 10UL * 60UL * 1000UL;
    bool armed_ = false;
    uint32_t since_ = 0;
    uint32_t starts_ = 0;
public:
    void connected(uint32_t now) { armed_ = true; since_ = now; }
    void disconnected() { armed_ = false; }
    void start() { armed_ = false; ++starts_; }
    uint32_t startRevision() const { return starts_; }
    void stop(uint32_t now) { armed_ = true; since_ = now; }
    void stopIfUnchanged(uint32_t now, uint32_t revision) {
        if (starts_ == revision) stop(now);
    }
    void defer(uint32_t now) { if (armed_) since_ = now; }
    void telemetry(uint32_t now, uint16_t speedHundredths) {
        if (speedHundredths != 0 && speedHundredths != 100) defer(now);
    }
    bool expired(uint32_t now, bool telemetryFresh) {
        if (!telemetryFresh) defer(now);
        return armed_ && telemetryFresh && uint32_t(now - since_) >= TimeoutMs;
    }
};
