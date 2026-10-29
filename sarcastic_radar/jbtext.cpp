#include <Arduino.h>
#include <string.h>
#include <math.h>

#define STBTT_assert(x)   ((void)0)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "JetBrainsMono-SemiBold.h"
#include "jbtext.h"
#include "utf8.h"

#define JB_ALPHA   100

#define JB_SCR_W_DEF   600
#define JB_SCR_H_DEF   400

#define JB_BUF_W   96
#define JB_BUF_H   96

namespace {
stbtt_fontinfo g_font;
bool g_ok = false;
void (*g_pix)(int x, int y, uint16_t color) = nullptr;
uint16_t g_color = 0xFFFF;

int g_scrW = JB_SCR_W_DEF;
int g_scrH = JB_SCR_H_DEF;

int g_advUnits  = 600;
int g_ascUnits  = 1020;

int g_capUnits  = 730;
const int UPEM  = 1000;

inline int mulPx(int units, int px) { return (units * px + UPEM / 2) / UPEM; }

unsigned char g_buf[JB_BUF_W * JB_BUF_H];
}

void jb::init() {
    memset(&g_font, 0, sizeof(g_font));
    g_ok = (stbtt_InitFont(&g_font, (const unsigned char*)JetBrainsMono_SemiBold, 0) != 0);
    if (!g_ok) {
        Serial.println("[jb] font not parsed, nothing will be drawn");
        return;
    }
    int as = 0, de = 0, lg = 0;
    stbtt_GetFontVMetrics(&g_font, &as, &de, &lg);
    if (as > 0) g_ascUnits = as;

    int adv0 = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&g_font, '0', &adv0, &lsb);
    if (adv0 > 0) g_advUnits = adv0;

    int hx0 = 0, hy0 = 0, hx1 = 0, hy1 = 0;
    stbtt_GetGlyphBitmapBox(&g_font, stbtt_FindGlyphIndex(&g_font, 'H'),
                            1.0f, 1.0f, &hx0, &hy0, &hx1, &hy1);
    if (hy0 < 0) g_capUnits = -hy0;

    Serial.printf("[jb] JetBrains Mono: advance %d/1000 em, ascent %d/1000, cap height %d/1000\n",
                  g_advUnits, g_ascUnits, g_capUnits);
}

bool jb::ready() { return g_ok; }

void jb::setPixel(void (*fn)(int x, int y, uint16_t color)) { g_pix = fn; }

void jb::setBounds(int w, int h) {
    if (w > 0) g_scrW = w;
    if (h > 0) g_scrH = h;
}

int jb::adv(int px)  { return mulPx(g_advUnits, px); }
int jb::cap(int px)  { return mulPx(g_capUnits, px); }
int jb::rowH(int px) { return mulPx(1180, px); }

float jb::advF(int px) { return (float)g_advUnits * (float)px / (float)UPEM; }

int jb::strW(const char* s, int px) {
    if (!s) return 0;

    return (int)lroundf(jb::advF(px) * (float)utf8::count(s));
}

static void drawGlyph(int cp, int x, int baselineY, float scale) {
    if (!g_pix) return;
    int glyph = stbtt_FindGlyphIndex(&g_font, cp);
    if (!glyph) return;

    int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
    stbtt_GetGlyphBitmapBox(&g_font, glyph, scale, scale, &ix0, &iy0, &ix1, &iy1);
    int iw = ix1 - ix0;
    int ih = iy1 - iy0;
    if (iw <= 0 || ih <= 0) return;
    if (iw > JB_BUF_W || ih > JB_BUF_H) return;

    stbtt_MakeGlyphBitmap(&g_font, g_buf, iw, ih, iw, scale, scale, glyph);

    for (int j = 0; j < ih; j++) {
        int Y = baselineY + iy0 + j;
        if (Y < 0 || Y >= g_scrH) continue;
        const unsigned char* row = g_buf + j * iw;
        for (int i = 0; i < iw; i++) {
            if (row[i] < JB_ALPHA) continue;
            int X = x + ix0 + i;
            if (X < 0 || X >= g_scrW) continue;
            g_pix(X, Y, g_color);
        }
    }
}

void jb::draw(const char* s, int x, int baselineY, int px, uint16_t color) {
    if (!g_ok || !g_pix || !s || px <= 0) return;
    g_color = color;

    const float scale = (float)px / (float)UPEM;
    float pen = (float)x;

    while (*s) {
        const uint32_t cp = utf8::next(&s);

        int advU = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&g_font, (int)cp, &advU, &lsb);

        if (advU <= 0) advU = g_advUnits;

        drawGlyph((int)cp, (int)(pen + 0.5f), baselineY, scale);
        pen += (float)advU * scale;
    }
}

void jb::drawTop(const char* s, int x, int topY, int px, uint16_t color) {
    jb::draw(s, x, topY + jb::cap(px), px, color);
}
