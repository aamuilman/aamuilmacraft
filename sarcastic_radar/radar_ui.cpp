#include "radar_ui.h"
#include "config.h"
#include "chrome.h"
#include "jbtext.h"
#include "palette.h"
#include "radar_card.h"
#include "radar_icons.h"
#include "radar_shape.h"
#include "terrain.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static constexpr uint16_t C_BG     = palette::BG;
static constexpr uint16_t C_NORMAL = palette::DATA;
static constexpr uint16_t C_ALERT  = palette::ALERT;
static constexpr uint16_t C_FIX    = palette::FIX;

static constexpr uint16_t C_VIP    = palette::VIP;
static constexpr uint16_t C_ZEN    = palette::ZEN;
static constexpr uint16_t C_ACCENT = palette::ACCENT;

static_assert(CARD_ICON == RADAR_ICON_W, "CARD_ICON does not match RADAR_ICON_W");

struct LabelBox { int x, y, w, h; };

static LabelBox g_labels[48];
static int      g_labelCount = 0;

static bool boxesOverlap(const LabelBox& a, const LabelBox& b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
             a.y + a.h <= b.y || b.y + b.h <= a.y);
}

static bool tryPlaceLabel(int px, int py, int w, int h, LabelBox& out) {
    const int g = MARK_CLEAR;
    const int offsets[LABEL_POSITIONS][2] = {
        {  g,      -h - g },
        { -g - w,  -h - g },
        {  g,       g     },
        { -g - w,   g     },
        {  g,      -h / 2 },
        { -g - w,  -h / 2 },
        { -w / 2,  -h - g },
        { -w / 2,   g     },
    };

    for (int i = 0; i < LABEL_POSITIONS; i++) {
        LabelBox b;
        b.x = px + offsets[i][0];
        b.y = py + offsets[i][1];
        b.w = w;
        b.h = h;

        if (b.x < BEZEL_INSET + 2) continue;
        if (b.y < HEADER_H + 2) continue;
        if (b.x + b.w > CARD_X - 10) continue;
        if (b.y + b.h > SCREEN_H - 2) continue;

        bool clash = false;
        for (int j = 0; j < g_labelCount; j++) {
            if (boxesOverlap(b, g_labels[j])) { clash = true; break; }
        }
        if (!clash) {
            out = b;
            if (g_labelCount < (int)(sizeof(g_labels) / sizeof(g_labels[0]))) {
                g_labels[g_labelCount++] = b;
            }
            return true;
        }
    }
    return false;
}

static void nearestOnBox(int px, int py, const LabelBox& b, int& ox, int& oy) {
    ox = px < b.x ? b.x : (px > b.x + b.w ? b.x + b.w : px);
    oy = py < b.y ? b.y : (py > b.y + b.h ? b.y + b.h : py);
}

static void drawLeader(LGFX_Sprite& g, int px, int py, int ox, int oy) {
    const float dx = (float)(ox - px);
    const float dy = (float)(oy - py);
    const float d  = sqrtf(dx * dx + dy * dy);
    if (d <= (float)MARK_CLEAR + 0.5f) return;

    const float k = (float)MARK_CLEAR / d;
    g.drawLine(px + (int)lroundf(dx * k), py + (int)lroundf(dy * k), ox, oy, C_FIX);
}

static float edgeDist(float ux, float uy) {
    float lo = 0.0f;
    float hi = (float)RADAR_HALF * 1.4143f;
    for (int i = 0; i < 24; i++) {
        const float mid = 0.5f * (lo + hi);
        if (radarshape::inside(mid * ux, mid * uy)) lo = mid; else hi = mid;
    }
    return lo;
}

static void drawRadarGrid(LGFX_Sprite& g, float radiusKm, const char* place) {
    const int px  = JB_PX_RING;
    const int cap = jb::cap(px);

    const int step = (radiusKm > 120.0f) ? 50 : (radiusKm > 60.0f) ? 25 : 10;

    for (int km = step; km <= (int)radiusKm; km += step) {
        const int e  = (int)lroundf(RADAR_HALF * ((float)km / radiusKm));
        const int rk = RADAR_CORNER_R - (RADAR_HALF - e);

        if (rk >= 2) {
            g.drawRoundRect(RADAR_CX - e, RADAR_CY - e, 2 * e + 1, 2 * e + 1,
                            rk, C_FIX);
        } else {
            g.drawRect(RADAR_CX - e, RADAR_CY - e, 2 * e + 1, 2 * e + 1, C_FIX);
        }

        char lbl[12];
        snprintf(lbl, sizeof(lbl), "%d", km);
        int w = jb::strW(lbl, px);
        jb::drawTop(lbl, RADAR_CX - 6 - w, RADAR_CY - e - cap / 2, px, C_FIX);
    }

    g.drawLine(RADAR_CX - RADAR_HALF, RADAR_CY, RADAR_CX + RADAR_HALF, RADAR_CY, C_FIX);
    g.drawLine(RADAR_CX, RADAR_CY - RADAR_HALF, RADAR_CX, RADAR_CY + RADAR_HALF, C_FIX);

    for (int deg = 0; deg < 360; deg += ROSE_STEP_DEG) {
        const bool major = (deg % ROSE_MAJOR_DEG) == 0;
        const int  len   = major ? ROSE_MAJOR_LEN : ROSE_MINOR_LEN;
        float a  = (float)deg * (float)PI / 180.0f;
        float dx = sinf(a), dy = -cosf(a);
        const float d = edgeDist(dx, dy);
        g.drawLine(RADAR_CX + (int)lroundf(dx * d),
                        RADAR_CY + (int)lroundf(dy * d),
                        RADAR_CX + (int)lroundf(dx * (d - (float)len)),
                        RADAR_CY + (int)lroundf(dy * (d - (float)len)),
                        C_FIX);
    }

    const int adv = jb::adv(px);
    jb::drawTop("N", RADAR_CX - adv / 2, RADAR_CY - RADAR_HALF - cap - 4, px, C_FIX);
    jb::drawTop("S", RADAR_CX - adv / 2, RADAR_CY + RADAR_HALF + 4, px, C_FIX);
    jb::drawTop("W", RADAR_CX - RADAR_HALF + 4, RADAR_CY - cap - 3, px, C_FIX);
    jb::drawTop("E", RADAR_CX + RADAR_HALF - 4 - adv, RADAR_CY - cap - 3, px, C_FIX);

    g.fillCircle(RADAR_CX, RADAR_CY, 4, C_FIX);
    if (place && place[0]) {
        jb::drawTop(place, RADAR_CX + 8, RADAR_CY + 4, JB_PX_SUBLABEL, C_FIX);
    }
}

static int velocityLen(int gs_kt) {
    if (gs_kt <= 0) return VEL_MIN_PX;
    int span = VEL_MAX_PX - VEL_MIN_PX;
    int len  = VEL_MIN_PX + span * gs_kt / VEL_FULL_KT;
    if (len > VEL_MAX_PX) len = VEL_MAX_PX;
    return len;
}

static void formatTag(char* out, size_t n, int alt_ft, int gs_kt) {
    char a[12];
    if (alt_ft < 0)           snprintf(a, sizeof(a), "GND");
    else if (alt_ft >= 10000) snprintf(a, sizeof(a), "%03d", alt_ft / 100);
    else                      snprintf(a, sizeof(a), "%d", alt_ft);

    if (gs_kt > 0) snprintf(out, n, "%s/%d", a, gs_kt);
    else           snprintf(out, n, "%s", a);
}

static uint16_t targetColor(const Target& t) {
    if (t.emergency)             return C_ALERT;
    if (cardIsBusinessJet(t.type)) return C_VIP;
    return C_NORMAL;
}

static void drawDart(LGFX_Sprite& g, int x, int y, float track_deg, bool filled,
                     uint16_t col) {
    const float r  = track_deg * (float)PI / 180.0f;
    const float sn = sinf(r);
    const float cs = cosf(r);

    auto X = [&](int f, int lat) { return x + (int)lroundf(f * sn + lat * cs); };
    auto Y = [&](int f, int lat) { return y - (int)lroundf(f * cs - lat * sn); };

    const int nx = X(DART_NOSE, 0),          ny = Y(DART_NOSE, 0);
    const int lx = X(DART_TAIL, -DART_HALF), ly = Y(DART_TAIL, -DART_HALF);
    const int rx = X(DART_TAIL,  DART_HALF), ry = Y(DART_TAIL,  DART_HALF);
    const int kx = X(DART_NOTCH, 0),         ky = Y(DART_NOTCH, 0);

    if (filled) {
        g.fillTriangle(nx, ny, lx, ly, kx, ky, col);
        g.fillTriangle(nx, ny, kx, ky, rx, ry, col);
    } else {
        g.drawTriangle(nx, ny, lx, ly, kx, ky, col);
        g.drawTriangle(nx, ny, kx, ky, rx, ry, col);
    }
}

struct Plot {
    int      idx;
    int      px, py;
    int      prio;
};

static void drawTargets(LGFX_Sprite& g, const Target* targets, int count,
                        float radiusNm) {
    static Plot plots[MAX_TARGETS];
    int n = 0;

    int nearTarget = -1, nearPrio = 0;

    for (int i = 0; i < count && n < MAX_TARGETS; i++) {
        const Target& t = targets[i];

        float rr = t.dst_nm * (RADAR_HALF / radiusNm);
        float a  = t.dir_deg * (float)PI / 180.0f;

        const int dxp =  (int)lroundf(rr * sinf(a));
        const int dyp = -(int)lroundf(rr * cosf(a));

        if (!radarshape::inside((float)dxp, (float)dyp)) continue;

        Plot& p = plots[n++];
        p.idx  = i;
        p.prio = (int)lroundf(rr);
        p.px = RADAR_CX + dxp;
        p.py = RADAR_CY + dyp;

        if (!t.emergency && t.alt_ft >= 0 && (nearTarget < 0 || p.prio < nearPrio)) {
            nearTarget = i;
            nearPrio   = p.prio;
        }
    }

    for (int k = 0; k < n; k++) {
        const Plot& p = plots[k];
        const Target& t = targets[p.idx];

        const uint16_t col = targetColor(t);

        if (!isnan(t.track_deg)) {
            float ta = t.track_deg * (float)PI / 180.0f;
            int vl = velocityLen(t.gs_kt);
            int vx = p.px + (int)lroundf(sinf(ta) * (float)vl);
            int vy = p.py - (int)lroundf(cosf(ta) * (float)vl);
            g.drawLine(p.px, p.py, vx, vy, col);
        }

        if (isnan(t.track_deg)) {
            if (t.alt_ft < 0)      g.drawCircle(p.px, p.py, 3, col);
            else                   g.fillCircle(p.px, p.py, 3, col);
        } else {
            drawDart(g, p.px, p.py, t.track_deg, t.alt_ft >= 0, col);
        }

        if (p.idx == nearTarget)   g.drawCircle(p.px, p.py, MARK_CLEAR, C_FIX);
    }

    g_labelCount = 0;

    for (int a = 1; a < n; a++) {
        Plot key = plots[a];
        int b = a - 1;
        while (b >= 0 && plots[b].prio > key.prio) {
            plots[b + 1] = plots[b];
            b--;
        }
        plots[b + 1] = key;
    }

    for (int k = 0; k < n; k++) {
        const Plot& p = plots[k];
        const Target& t = targets[p.idx];

        const uint16_t col = targetColor(t);

        char sub[24];
        formatTag(sub, sizeof(sub), t.alt_ft, t.gs_kt);

        const int w1 = jb::strW(t.callsign, JB_PX_LABEL);
        const int w2 = jb::strW(sub, JB_PX_SUBLABEL);
        const int bw = (w1 > w2) ? w1 : w2;
        const int bh = jb::rowH(JB_PX_LABEL) + jb::rowH(JB_PX_SUBLABEL);

        LabelBox lb;
        if (tryPlaceLabel(p.px, p.py, bw, bh, lb)) {
            int ox, oy;
            nearestOnBox(p.px, p.py, lb, ox, oy);
            drawLeader(g, p.px, p.py, ox, oy);

            jb::drawTop(t.callsign, lb.x, lb.y, JB_PX_LABEL, col);
            jb::drawTop(sub, lb.x, lb.y + jb::rowH(JB_PX_LABEL),
                        JB_PX_SUBLABEL, C_FIX);
        } else {
            const int sw = jb::strW(t.callsign, JB_PX_SUBLABEL);
            LabelBox sb;
            if (tryPlaceLabel(p.px, p.py, sw, jb::rowH(JB_PX_SUBLABEL), sb)) {
                jb::drawTop(t.callsign, sb.x, sb.y, JB_PX_SUBLABEL, col);
            }
        }
    }
}

static constexpr uint16_t C_PLATE_INK = WHITE;

static constexpr int ICON_STRIDE = (RADAR_ICON_W + 7) / 8;

static void drawIcon(LGFX_Sprite& g, int x, int y, const uint8_t* mask,
                     uint16_t ink) {
    for (int row = 0; row < RADAR_ICON_H; row++) {
        const uint8_t* line = mask + row * ICON_STRIDE;
        int runStart = -1;

        for (int bit = 0; bit <= RADAR_ICON_W; bit++) {
            const bool lit = (bit < RADAR_ICON_W) &&
                             (((line[bit >> 3] >> (7 - (bit & 7))) & 1) != 0);
            if (lit) {
                if (runStart < 0) runStart = bit;
            } else if (runStart >= 0) {
                g.fillRect(x + runStart, y + row, bit - runStart, 1, ink);
                runStart = -1;
            }
        }
    }
}

static void drawField(LGFX_Sprite& g, int y, const char* label, const char* value) {
    jb::drawTop(label, CARD_TEXT_X, y, JB_PX_CARD_LABEL, C_FIX);

    const int vx   = CARD_TEXT_X + CARD_LABEL_W;
    const int maxW = CARD_TEXT_R - vx;
    int maxCh = (int)((float)maxW / jb::advF(JB_PX_CARD_VALUE));
    if (maxCh < 0) maxCh = 0;
    if (maxCh > 31) maxCh = 31;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.*s", maxCh, value);
    jb::drawTop(buf, vx, y, JB_PX_CARD_VALUE, C_NORMAL);
}

static void drawValuePlate(LGFX_Sprite& g, int x, int y, int w, const char* value) {
    g.fillRect(x, y, w, CARD_VALUE_H, C_ACCENT);

    const int vw = jb::strW(value, JB_PX_CARD_VALUE);
    jb::draw(value, x + (w - vw) / 2,
             y + (CARD_VALUE_H + jb::cap(JB_PX_CARD_VALUE)) / 2,
             JB_PX_CARD_VALUE, C_NORMAL);
}

static void drawQuote(LGFX_Sprite& g, int y, const char* text) {
    const int px     = JB_PX_CARD_QUOTE;
    const int indent = jb::adv(px);

    char lines[CARD_QUOTE_LINES][CARD_LINE_MAX];
    const int n = cardWrap(text, px, CARD_TEXT_W - indent, lines, CARD_QUOTE_LINES);

    for (int i = 0; i < n; i++) {
        const int ly = y + i * jb::rowH(px);
        if (i == 0) jb::drawTop(">", CARD_TEXT_X, ly, px, C_NORMAL);
        jb::drawTop(lines[i], CARD_TEXT_X + indent, ly, px, C_NORMAL);
    }
}

static void drawCard(LGFX_Sprite& g, const CardInfo& info) {
    const char*    title;
    uint16_t       plateCol;
    const uint8_t* icon = nullptr;
    switch (info.kind) {
        case CardKind::Emergency:
            title    = "EMERGENCY";
            plateCol = C_ALERT;
            break;
        case CardKind::Business:
            title    = "CELEBRITY ALERT";
            plateCol = C_VIP;
            icon     = &RADAR_ICON_GUEST_STAR[0][0];
            break;
        case CardKind::Window:
            title    = "NOW FLY";
            plateCol = C_ZEN;
            icon     = &RADAR_ICON_SEAT[0][0];
            break;
        default:
            return;
    }

    g.fillRect(CARD_X, CARD_BODY_TOP, CARD_W, CARD_PLATE_H, plateCol);

    const int ty = CARD_BODY_TOP + (CARD_PLATE_H - jb::cap(JB_PX_CARD_PLATE)) / 2;
    int tx;

    if (icon) {
        drawIcon(g, CARD_X + CARD_PLATE_PAD, CARD_BODY_TOP + CARD_PLATE_PAD,
                 icon, C_PLATE_INK);

        tx = CARD_TITLE_X;
    } else {
        tx = CARD_X + (CARD_W - jb::strW(title, JB_PX_CARD_PLATE)) / 2;
    }
    jb::drawTop(title, tx, ty, JB_PX_CARD_PLATE, C_PLATE_INK);

    drawField(g, CARD_ROWS_TOP, "AIRCRAFT", info.bort);
    drawField(g, CARD_ROWS_TOP + jb::rowH(JB_PX_CARD_VALUE), "CALLSIGN", info.callsign);

    const int pw = (CARD_W - CARD_VALUE_GAP) / 2;
    drawValuePlate(g, CARD_X, CARD_VALUES_TOP, pw, info.alt);
    drawValuePlate(g, CARD_X + pw + CARD_VALUE_GAP, CARD_VALUES_TOP, pw, info.spd);

    char phrase[96];
    if (cardPhraseText(info.kind, info.phrase, info.callsign,
                       phrase, sizeof(phrase)) > 0) {
        drawQuote(g, CARD_QUOTE_TOP, phrase);
    }
}

static void drawRecords(LGFX_Sprite& g, const CardRecords& r) {
    const int y = CARD_RECORDS_TOP;
    g.drawLine(CARD_TEXT_X, y, CARD_TEXT_R, y, C_FIX);

    const int ry0 = y + CARD_RULE_GAP;

    if (!r.far.has && !r.high.has && !r.fast.has && !r.near.has) {
        jb::drawTop("no aircraft in the air", CARD_TEXT_X, ry0,
                    JB_PX_CARD_RECORD, C_NORMAL);
        return;
    }

    const struct Row { const char* label; const CardRecord* rec; } rows[CARD_RECORDS] = {
        { "FAR",  &r.far  },
        { "HIGH", &r.high },
        { "FAST", &r.fast },
        { "NEAR", &r.near },
    };

    for (int i = 0; i < CARD_RECORDS; i++) {
        const CardRecord& cr = *rows[i].rec;
        if (cr.has) {
            const int ry = ry0 + i * jb::rowH(JB_PX_CARD_RECORD);
            jb::drawTop(rows[i].label, CARD_TEXT_X, ry, JB_PX_CARD_RECORD, C_FIX);
            jb::drawTop(cr.callsign, CARD_TEXT_X + CARD_LABEL_W, ry,
                        JB_PX_CARD_RECORD, C_NORMAL);
            jb::drawTop(cr.value, CARD_TEXT_R - jb::strW(cr.value, JB_PX_CARD_RECORD),
                        ry, JB_PX_CARD_RECORD, C_NORMAL);
        }
    }
}

static void drawMetarBar(LGFX_Sprite& g, const RadarStatus& st) {
    g.fillRect(CARD_X, METAR_TOP, CARD_W, METAR_BLOCK_H, C_NORMAL);

    const int px = JB_PX_METAR;
    const int tx = CARD_TEXT_X;

    if (!st.metarOk) {
        jb::drawTop("no METAR", tx, METAR_TOP + CARD_PAD, px, C_BG);
        return;
    }

    char lines[METAR_LINES][CARD_LINE_MAX];
    const int n = cardWrap(st.metar, px, CARD_TEXT_W, lines, METAR_LINES);

    for (int i = 0; i < n; i++) {
        jb::drawTop(lines[i], tx, METAR_TOP + CARD_PAD + i * jb::rowH(px), px, C_BG);
    }
}

static void drawState(LGFX_Sprite& g, const char* title, uint16_t titleCol,
                      const char* why) {
    jb::drawTop(title, CARD_TEXT_X, CARD_BODY_TOP, JB_PX_CARD_VALUE, titleCol);
    if (!why || !why[0]) return;

    char lines[CARD_WRAP_LINES][CARD_LINE_MAX];
    const int n = cardWrap(why, JB_PX_CARD_QUOTE, CARD_TEXT_W, lines, CARD_WRAP_LINES);

    int y = CARD_BODY_TOP + jb::rowH(JB_PX_CARD_VALUE) + CARD_ROW_GAP;
    for (int i = 0; i < n; i++) {
        jb::drawTop(lines[i], CARD_TEXT_X, y, JB_PX_CARD_QUOTE, C_FIX);
        y += jb::rowH(JB_PX_CARD_QUOTE);
    }
}

static void drawColumn(LGFX_Sprite& g, const Target* targets, int count,
                       const RadarStatus& st) {
    if (!st.fetchOk) {
        char title[20];
        if (st.everOk) {
            const unsigned age = st.ageMin > 99 ? 99 : (unsigned)st.ageMin;
            snprintf(title, sizeof(title), "NO DATA %uM", age);
        } else {
            snprintf(title, sizeof(title), "NO DATA");
        }
        drawState(g, title, C_ALERT, st.error);
        drawMetarBar(g, st);
        return;
    }

    const int idx = cardPickIndex(targets, count);
    if (idx < 0) {
        drawState(g, "NO TRAFFIC", C_NORMAL, "no aircraft in range");
    } else {
        drawCard(g, cardBuild(targets[idx]));
        drawRecords(g, cardRecords(targets, count));
    }

    drawMetarBar(g, st);
}

static LGFX_Sprite* g_terrainInkTarget = nullptr;

static void terrainInk(int x, int y, uint16_t color) {
    if (g_terrainInkTarget) g_terrainInkTarget->drawPixel(x, y, color);
}

void radarPaint(LGFX_Sprite& g, const Target* targets, int count,
                const RadarStatus& st) {
    g.fillSprite(C_BG);

    terrain::setPixel(terrainInk);
    g_terrainInkTarget = &g;
    terrain::draw(RADAR_CX - RADAR_HALF, RADAR_CY - RADAR_HALF);
    g_terrainInkTarget = nullptr;

    g.drawRect(BEZEL_INSET, BEZEL_INSET,
               SCREEN_W - 2 * BEZEL_INSET, SCREEN_H - 2 * BEZEL_INSET, C_FIX);

    const int px  = JB_PX_HEADER;
    const int adv = jb::adv(px);
    const int hy  = (HEADER_H - jb::cap(px)) / 2;

    jb::drawTop(st.place[0] ? st.place : "RADAR", BEZEL_INSET + 6, hy, px, C_FIX);

    char range[16];
    snprintf(range, sizeof(range), "%.0fKM", (double)st.radiusKm);
    jb::drawTop(range, BEZEL_INSET + 6 + adv * (int)strlen(st.place[0] ? st.place : "RADAR") + adv / 2,
                hy, px, C_NORMAL);

    int right = SCREEN_W - BEZEL_INSET - 6;
    right = chrome::drawClock(right, hy, st.zuluStr, st.timeStr, st.timeOk);

    const chrome::Power pw = { st.battPct, st.extPower, st.chargeTrend };
    right = chrome::drawPower(right, hy, pw);

    char data[24];
    uint16_t dataCol = C_NORMAL;
    if (!st.fetchOk) {
        if (st.everOk) {
            unsigned age = st.ageMin > 99 ? 99 : (unsigned)st.ageMin;
            snprintf(data, sizeof(data), "NO DATA %uM", age);
        } else {
            snprintf(data, sizeof(data), "NO DATA");
        }
        dataCol = C_ALERT;
    } else {
        if (st.total > (uint32_t)count) {
            snprintf(data, sizeof(data), "OK %d/%u", count, (unsigned)st.total);
        } else {
            snprintf(data, sizeof(data), "OK %d AC", count);
        }
    }
    jb::drawTop(data, right - jb::strW(data, px), hy, px, dataCol);

    chrome::drawRule(SCREEN_W, HEADER_H);

    const float radiusNm = st.radiusKm / 1.852f;

    drawRadarGrid(g, st.radiusKm, st.place);
    drawTargets(g, targets, count, radiusNm);
    drawColumn(g, targets, count, st);
}

void radarPaintPowerOff(LGFX_Sprite& g) {
    g.fillSprite(C_BG);

    const int cx = SCREEN_W / 2;

    const char* title = "POWER OFF";
    jb::drawTop(title, cx - jb::strW(title, JB_PX_OFF) / 2,
                SCREEN_H / 2 - jb::rowH(JB_PX_OFF), JB_PX_OFF, C_NORMAL);

    const char* hint = "Press the PWR button to start";
    const int hp = 16;
    jb::drawTop(hint, cx - jb::strW(hint, hp) / 2,
                SCREEN_H / 2 + 20, hp, C_FIX);
}
