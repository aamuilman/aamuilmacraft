#pragma once

#include <stdint.h>

struct TerrainResult {
    bool     ok;
    bool     tried;
    uint8_t  tiles;
    uint32_t ms;
    char     error[48];
};

namespace terrain {
void setPixel(void (*fn)(int x, int y, uint16_t color));

bool ready();

bool build(double lat, double lon, float radiusKm, TerrainResult& out);

void draw(int originX, int originY);
}
