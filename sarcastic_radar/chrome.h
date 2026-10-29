#pragma once

#include <stdint.h>

namespace chrome {
struct Power {
    int  battPct;
    bool extPower;

    int  chargeTrend;
};

void drawBattery(int x, int y, int pct);

int drawPower(int right, int y, const Power& p);

int drawClock(int right, int y, const char* zulu, const char* local, bool timeOk);

void drawRule(int w, int headerH);
}
