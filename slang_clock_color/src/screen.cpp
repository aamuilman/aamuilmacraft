#include "screen.h"
#include "../config.h"

namespace screen {
namespace {

const int W = EPD_WIDTH;
const int H = EPD_HEIGHT;

Pixel g_pixel = nullptr;
bool  g_ready = false;

void put(int x, int y, uint16_t c) {
    if (g_pixel) g_pixel(x, y, c);
}

void ensureInit() {
    if (!g_ready) {
        txt::init();
        g_ready = true;
    }
}

txt::Palette paletteFor(slang::Context ctx) {
    txt::Palette p;
    p.bg          = pal::WHITE;
    p.ink         = pal::BLACK;
    p.solidFill   = pal::RED;
    p.solidInk    = pal::WHITE;
    p.outlineFill = pal::YELLOW;
    p.outlineInk  = pal::BLACK;
    p.punchFill   = pal::BLACK;
    p.punchInk    = pal::WHITE;

    switch (ctx) {
    case slang::CTX_EVENING_CHILL: p.punchFill = pal::BLUE;  break;
    case slang::CTX_FRIDAY_PARTY:  p.punchFill = pal::RED;   break;
    case slang::CTX_WEEKEND:       p.punchFill = pal::GREEN; break;
    default: break;
    }
    return p;
}

}
void setPixel(Pixel p) { g_pixel = p; }

void draw(const Data& d) {
    ensureInit();
    const txt::Palette t = paletteFor(d.ctx);

    txt::setCanvas({ put, 0, 0, W - 1, H - 1 });
    txt::fillRect(0, 0, W, H, t.bg);

    const int contentW = W - 2 * MARGIN_X;

    int cardH = 0;
    int cardTop = 0;
    int heroBottom = H - MARGIN_BOTTOM;

    if (d.punch && *d.punch) {
        cardH = txt::punchHeight(d.punch, contentW);
        cardTop = H - MARGIN_BOTTOM - cardH;
        heroBottom = cardTop - CARD_GAP;
    }

    const int heroH = heroBottom - HERO_TOP;
    if (heroH > 24 && d.hero && *d.hero) {
        txt::drawHero(d.hero, MARGIN_X, HERO_TOP, contentW, heroH, t,
                      true, HERO_MAX_LINES);
    }

    if (cardH > 0 && d.punch && *d.punch) {
        txt::drawPunchCard(d.punch, MARGIN_X, cardTop, contentW,
                           t.punchFill, t.punchInk);
    }
}

void drawPowerOff() {
    ensureInit();
    const txt::Palette t = paletteFor(slang::CTX_NIGHT);

    txt::setCanvas({ put, 0, 0, W - 1, H - 1 });
    txt::fillRect(0, 0, W, H, t.bg);

    const int contentW = W - 2 * MARGIN_X;
    const int top = 70;
    const int split = 300;

    txt::drawHero("POWER OFF", MARGIN_X, top, contentW, split - top,
                  t, true, 2);
    txt::drawHero("TO TURN ON PRESS [PWR]", MARGIN_X, split, contentW,
                  H - split - MARGIN_BOTTOM, t, true, 3);
}

}