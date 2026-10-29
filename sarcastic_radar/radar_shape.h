#pragma once

#include "config.h"

namespace radarshape {
inline bool inside(float dx, float dy) {
    const float half = (float)RADAR_HALF;
    const float r    = (float)RADAR_CORNER_R;

    const float ax = dx < 0.0f ? -dx : dx;
    const float ay = dy < 0.0f ? -dy : dy;
    if (ax > half || ay > half) return false;

    const float ix = ax - (half - r);
    const float iy = ay - (half - r);
    if (ix <= 0.0f || iy <= 0.0f) return true;

    return ix * ix + iy * iy <= r * r;
}
}
