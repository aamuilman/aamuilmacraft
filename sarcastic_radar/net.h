#pragma once

#include <stdint.h>

#include "adsb.h"
#include "device.h"
#include "metar.h"
#include "settings.h"
#include "terrain.h"

struct NetResult {
    uint32_t       seq;
    bool           answered;
    bool           wifiOk;
    bool           radioDown;
    FetchResult    fetch;
    Metar          metar;
    TerrainResult  terrain;
    dev::NtpResult ntp;
};

namespace net {
bool begin(const Settings& cfg);

void shutdown();

void assertOwner(const char* who);

void fetchTargets(Target* out, int max, NetResult& res, bool wantMetar,
                  bool wantTerrain);
}
