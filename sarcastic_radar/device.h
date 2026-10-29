#pragma once

#include <stdint.h>
#include <time.h>

struct DeviceStatus {
    char timeStr[6];
    char zuluStr[8];

    bool timeOk;

    int  battPct;
    bool extPower;

    int  chargeTrend;
};

namespace dev {
void begin(int utcOffsetMin);

void pollPower();

void samplePower();

bool takePowerChanged();

const DeviceStatus& status();

void refreshClock();

struct NtpResult {
    bool      attempted;
    bool      answered;
    bool      rtcPlausible;
    uint32_t  waitedMs;
    struct tm utc;
};

void applyTimeSync(const NtpResult& r);
}
