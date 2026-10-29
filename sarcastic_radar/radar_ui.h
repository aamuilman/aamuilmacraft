#pragma once

#include <stdint.h>
#include <M5GFX.h>
#include "adsb.h"

#include "metar.h"

struct RadarStatus {
    bool     fetchOk;
    bool     wifiOk;
    uint32_t total;

    char     timeStr[6];
    char     zuluStr[8];

    bool     timeOk;
    char     error[40];

    int  battPct;
    bool extPower;

    int  chargeTrend;

    bool     everOk;
    uint32_t ageMin;

    bool     metarOk;
    char     metar[METAR_RAW_MAX];

    float radiusKm;
    char  place[10];
};

void radarPaint(LGFX_Sprite& g, const Target* targets, int count,
                const RadarStatus& st);

void radarPaintPowerOff(LGFX_Sprite& g);
