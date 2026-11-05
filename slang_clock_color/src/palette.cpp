#include "palette.h"

#include <math.h>

namespace {

float g_lin[256];
float g_inv[1025];
bool  g_ready = false;

inline int expand5(int v) { return (v << 3) | (v >> 2); }
inline int expand6(int v) { return (v << 2) | (v >> 4); }

void ensureTables() {
    if (g_ready) return;

    for (int i = 0; i < 256; ++i) {
        const float s = (float)i / 255.0f;
        g_lin[i] = (s <= 0.04045f) ? (s / 12.92f)
                                   : powf((s + 0.055f) / 1.055f, 2.4f);
    }

    for (int i = 0; i <= 1024; ++i) {
        const float x = (float)i / 1024.0f;
        const float s = (x <= 0.0031308f) ? (x * 12.92f)
                                          : (1.055f * powf(x, 1.0f / 2.4f) - 0.055f);
        g_inv[i] = s * 255.0f;
    }

    g_ready = true;
}

inline float toSrgb(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 255.0f;
    const float f = x * 1024.0f;
    const int   i = (int)f;
    const float t = f - (float)i;
    return g_inv[i] + (g_inv[i + 1] - g_inv[i]) * t;
}

}
void pal::toLinear565(uint16_t p, float& r, float& g, float& b) {
    ensureTables();
    r = g_lin[expand5((p >> 11) & 31)];
    g = g_lin[expand6((p >>  5) & 63)];
    b = g_lin[expand5( p        & 31)];
}

uint16_t pal::fromLinear565(float r, float g, float b) {
    ensureTables();

    const int r8 = (int)(toSrgb(r) + 0.5f);
    const int g8 = (int)(toSrgb(g) + 0.5f);
    const int b8 = (int)(toSrgb(b) + 0.5f);

    const int r5 = (r8 * 31 + 127) / 255;
    const int g6 = (g8 * 63 + 127) / 255;
    const int b5 = (b8 * 31 + 127) / 255;

    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}
