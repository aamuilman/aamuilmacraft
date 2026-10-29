#include "chrome.h"
#include "config.h"
#include "jbtext.h"
#include "palette.h"
#include "ui.h"

#include <stdio.h>

void chrome::drawBattery(int x, int y, int pct) {
    LGFX_Sprite& g = ui::canvas();

    uint16_t col = palette::DATA;
    if (pct >= 0 && pct <= 30) col = palette::ALERT;

    g.drawRect(x, y, STATUS_BATT_W, STATUS_BATT_H, col);

    const int nubH = STATUS_BATT_NUB_W * 2;
    g.fillRect(x + STATUS_BATT_W, y + (STATUS_BATT_H - nubH) / 2,
               STATUS_BATT_NUB_W, nubH, col);

    if (pct > 0) {
        if (pct > 100) pct = 100;
        const int innerW = STATUS_BATT_W - 4;
        int fillW = innerW * pct / 100;

        if (fillW < 1) fillW = 1;
        g.fillRect(x + 2, y + 2, fillW, STATUS_BATT_H - 4, col);
    }
}

static void drawTrend(int x, int y, bool up) {
    LGFX_Sprite& g = ui::canvas();

    if (up) {
        g.fillTriangle(x, y + STATUS_ARROW_H,
                       x + STATUS_ARROW_W / 2, y,
                       x + STATUS_ARROW_W, y + STATUS_ARROW_H,
                       palette::FIX);
    } else {
        g.fillTriangle(x, y,
                       x + STATUS_ARROW_W / 2, y + STATUS_ARROW_H,
                       x + STATUS_ARROW_W, y,
                       palette::ALERT);
    }
}

int chrome::drawPower(int right, int y, const Power& p) {
    const int px = JB_PX_HEADER;

    if (p.chargeTrend != 0) {
        drawTrend(right - STATUS_ARROW_W, y + 2, p.chargeTrend > 0);
        right -= STATUS_ARROW_W + STATUS_ARROW_GAP;
    }

    if (p.extPower) {
        jb::drawTop("USB", right - jb::strW("USB", px), y, px, palette::FIX);
        right -= jb::strW("USB", px) + STATUS_GAP;
    }

    char pct[16];
    if (p.battPct < 0) snprintf(pct, sizeof(pct), "--%%");
    else               snprintf(pct, sizeof(pct), "%d%%", p.battPct);
    jb::drawTop(pct, right - jb::strW(pct, px), y, px, palette::DATA);
    right -= jb::strW(pct, px) + STATUS_BATT_GAP;

    drawBattery(right - STATUS_BATT_W, y, p.battPct);
    right -= STATUS_BATT_W + STATUS_GAP;

    return right;
}

int chrome::drawClock(int right, int y, const char* zulu, const char* local,
                      bool timeOk) {
    const int px = JB_PX_HEADER;

    jb::drawTop(zulu, right - jb::strW(zulu, px), y, px,
                timeOk ? palette::FIX : palette::ALERT);
    right -= jb::strW(zulu, px) + STATUS_GAP;

    jb::drawTop(local, right - jb::strW(local, px), y, px,
                timeOk ? palette::DATA : palette::ALERT);
    right -= jb::strW(local, px) + STATUS_GAP;

    return right;
}

void chrome::drawRule(int w, int headerH) {
    ui::canvas().drawLine(BEZEL_INSET, headerH, w - BEZEL_INSET, headerH,
                          palette::FIX);
}
