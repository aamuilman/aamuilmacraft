#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STBTT_assert(x) ((void)0)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "text_render.h"
#include "../config.h"
#include "fonts/unbounded_black.h"
#include "fonts/jetbrains_mono.h"

namespace txt {
namespace {

struct Font {
    stbtt_fontinfo info;
    bool ok = false;
    float capRatio = 0.75f;
};

Font g_hero;
Font g_mono;
Canvas g_canvas = { nullptr, 0, 0, 0, 0 };

const int kHeroMaxPx  = HERO_MAX_PX;
const int kHeroMinPx  = HERO_MIN_PX;
const int kPunchMaxPx = PUNCH_MAX_PX;
const int kPunchMinPx = PUNCH_MIN_PX;
const int kPunchMaxLn = PUNCH_MAX_LINES;
const int kPunchMaxH  = PUNCH_MAX_H;
const int kBadgeBorder = 2;

const int kGlyphBuf = 128;

inline int imax(int a, int b) { return a > b ? a : b; }
inline int imin(int a, int b) { return a < b ? a : b; }
inline int iround(float v) { return (int)(v + 0.5f); }

void putPx(int x, int y, uint16_t c) {
    if (!g_canvas.pixel) return;
    if (x < g_canvas.x0 || x > g_canvas.x1 ||
        y < g_canvas.y0 || y > g_canvas.y1) return;
    g_canvas.pixel(x, y, c);
}

int decodeUtf8(const char*& s) {
    const unsigned char c = (unsigned char)*s++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *s) {
        return ((c & 0x1F) << 6) | ((unsigned char)*s++ & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && s[0] && s[1]) {
        int cp = ((c & 0x0F) << 12) | (((unsigned char)s[0] & 0x3F) << 6)
               | ((unsigned char)s[1] & 0x3F);
        s += 2;
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && s[0] && s[1] && s[2]) {
        int cp = ((c & 0x07) << 18) | (((unsigned char)s[0] & 0x3F) << 12)
               | (((unsigned char)s[1] & 0x3F) << 6) | ((unsigned char)s[2] & 0x3F);
        s += 3;
        return cp;
    }
    return '?';
}

int upperCp(int cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 'a' + 'A';
    return cp;
}

float scaleFor(const Font& f, int px) {
    return stbtt_ScaleForMappingEmToPixels(&f.info, (float)px);
}

int ascentPx(const Font& f, int px) {
    int a = 0, d = 0, g = 0;
    stbtt_GetFontVMetrics(&f.info, &a, &d, &g);
    return iround((float)a * scaleFor(f, px));
}

int capPx(const Font& f, int px) {
    return iround((float)px * f.capRatio);
}

int advanceUnits(const Font& f, int cp) {
    int a = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&f.info, cp, &a, &lsb);
    return a;
}

void drawGlyph(const Font& f, int cp, int x, int baseline, float scale,
               uint16_t color) {
    int glyph = stbtt_FindGlyphIndex(&f.info, cp);
    if (!glyph) return;

    int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
    stbtt_GetGlyphBitmapBox(&f.info, glyph, scale, scale, &ix0, &iy0, &ix1, &iy1);
    const int iw = ix1 - ix0;
    const int ih = iy1 - iy0;
    if (iw <= 0 || ih <= 0) return;

    static unsigned char buf[kGlyphBuf * kGlyphBuf];
    if (iw > kGlyphBuf || ih > kGlyphBuf) return;
    stbtt_MakeGlyphBitmap(&f.info, buf, iw, ih, iw, scale, scale, glyph);

    for (int j = 0; j < ih; j++) {
        const int Y = baseline + iy0 + j;
        for (int i = 0; i < iw; i++) {
            if (buf[j * iw + i] < 100) continue;
            putPx(x + ix0 + i, Y, color);
        }
    }
}

int widthN(const Font& f, const char* s, int n, int px, bool upper) {
    const float scale = scaleFor(f, px);
    float w = 0.0f;
    const char* p = s;
    const char* end = s + n;
    while (p < end && *p) {
        int cp = decodeUtf8(p);
        if (p > end) break;
        if (upper) cp = upperCp(cp);
        w += (float)advanceUnits(f, cp) * scale;
    }
    return iround(w);
}

int widthZ(const Font& f, const char* s, int px, bool upper = false) {
    return widthN(f, s, (int)strlen(s), px, upper);
}

void drawTextN(const Font& f, const char* s, int n, int x, int baseline,
               int px, uint16_t color, bool upper) {
    const float scale = scaleFor(f, px);
    float pen = (float)x;
    const char* p = s;
    const char* end = s + n;
    while (p < end && *p) {
        int cp = decodeUtf8(p);
        if (p > end) break;
        if (upper) cp = upperCp(cp);
        drawGlyph(f, cp, iround(pen), baseline, scale, color);
        pen += (float)advanceUnits(f, cp) * scale;
    }
}

void fillRectRaw(int x, int y, int w, int h, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            putPx(x + i, y + j, c);
}

void strokeRectRaw(int x, int y, int w, int h, int t, uint16_t c) {
    if (w <= 0 || h <= 0 || t <= 0) return;
    fillRectRaw(x, y, w, t, c);
    fillRectRaw(x, y + h - t, w, t, c);
    fillRectRaw(x, y, t, h, c);
    fillRectRaw(x + w - t, y, t, h, c);
}

enum TokKind : uint8_t { TOK_WORD = 0, TOK_SOLID = 1, TOK_OUTLINE = 2 };

struct Tok {
    uint8_t kind;
    bool spaceBefore;
    const char* s;
    int n;
    int w;
    int bpx;
    int tail;
};

struct Line {
    int first;
    int count;
    int w;
};

struct Layout {
    Tok toks[64];
    int nt;
    Line lines[10];
    int nl;
    int totalLines;
    int px;
    int lineH;
    int height;
    int ascent;
    int cap;
};

int spaceW(const Font& f, int px) {
    int w = widthZ(f, " ", px);
    return w > 0 ? w : imax(2, px / 4);
}

int parseRich(const char* s, Tok* toks, int maxToks) {
    int nt = 0;
    bool pendingSpace = false;

    while (*s && nt < maxToks) {
        if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
            pendingSpace = true;
            s++;
            continue;
        }

        if (*s == '[') {
            const char* close = strchr(s + 1, ']');
            if (close) {
                const char* cs = s + 1;
                const char* ce = close;
                while (cs < ce && *cs == ' ') cs++;
                while (ce > cs && ce[-1] == ' ') ce--;
                if (ce > cs) {
                    toks[nt++] = { TOK_SOLID, pendingSpace, cs,
                                   (int)(ce - cs), 0, 0, 0 };
                }
                s = close + 1;
                pendingSpace = false;
                continue;
            }
            s++;
            continue;
        }

        if ((unsigned char)*s == 0x01) {
            const char* close = strchr(s + 1, '\x02');
            if (close) {
                const char* cs = s + 1;
                const char* ce = close;
                while (cs < ce && *cs == ' ') cs++;
                while (ce > cs && ce[-1] == ' ') ce--;
                if (ce > cs) {
                    toks[nt++] = { TOK_OUTLINE, pendingSpace, cs,
                                   (int)(ce - cs), 0, 0, 0 };
                }
                s = close + 1;
                pendingSpace = false;
                continue;
            }
            s++;
            continue;
        }

        const char* start = s;
        while (*s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' &&
               *s != '[' && (unsigned char)*s != 0x01) {
            s++;
        }
        if (s > start) {
            toks[nt++] = { TOK_WORD, pendingSpace, start,
                           (int)(s - start), 0, 0, 0 };
            pendingSpace = false;
        } else {
            s++;
        }
    }
    return nt;
}

int badgeWidth(const Font& f, const Tok& t, int bpx) {
    const int padX = imax(2, bpx / 10);
    return widthN(f, t.s, t.n, bpx, true) + 2 * padX + 2 * kBadgeBorder;
}

int badgeHeight(const Font& f, int bpx) {
    const int padY = imax(1, bpx / 14);
    return capPx(f, bpx) + 2 * padY + 2 * kBadgeBorder;
}

bool isTrailingPunct(const Tok& t) {
    if (t.kind != TOK_WORD || t.spaceBefore || t.n < 1 || t.n > 2) return false;
    for (int k = 0; k < t.n; k++) {
        const char c = t.s[k];
        if (c != ',' && c != '.' && c != '!' &&
            c != '?' && c != ';' && c != ':') {
            return false;
        }
    }
    return true;
}

void layoutAt(const char* rich, const Font& f, int px, int maxW,
              int maxLines, Layout& L) {
    L.nt = parseRich(rich, L.toks, 64);
    L.px = px;
    L.nl = 0;
    L.ascent = ascentPx(f, px);
    L.cap = capPx(f, px);

    const int sp = spaceW(f, px);
    int maxBadgeH = 0;

    for (int i = 0; i < L.nt; i++) {
        Tok& t = L.toks[i];
        if (t.kind == TOK_WORD) {
            t.bpx = px;
            t.w = widthN(f, t.s, t.n, px, true);
        } else {
            int reserve = 0;
            if (i + 1 < L.nt && isTrailingPunct(L.toks[i + 1])) {
                const Tok& nx = L.toks[i + 1];
                reserve = widthN(f, nx.s, nx.n, px, true);
            }

            int bpx = px;
            int bw = badgeWidth(f, t, bpx);
            while (bw + reserve > maxW && bpx > 7) {
                bpx--;
                bw = badgeWidth(f, t, bpx);
            }
            t.bpx = bpx;
            t.w = bw;
            maxBadgeH = imax(maxBadgeH, badgeHeight(f, bpx));
        }
    }

    for (int i = 0; i < L.nt; i++) {
        L.toks[i].tail = 0;
        if (i + 1 < L.nt && isTrailingPunct(L.toks[i + 1])) {
            const Tok& nx = L.toks[i + 1];
            L.toks[i].tail = widthN(f, nx.s, nx.n, px, true);
        }
    }

    L.lineH = imax(iround((float)px * 1.10f), maxBadgeH + 1);

    int first = 0;
    int count = 0;
    int lineW = 0;

    for (int i = 0; i < L.nt; i++) {
        const bool spaced = (count > 0 && L.toks[i].spaceBefore);
        const int sep = spaced ? sp : 0;
        if (count > 0 && spaced &&
            lineW + sep + L.toks[i].w + L.toks[i].tail > maxW) {
            if (L.nl < 10) L.lines[L.nl++] = { first, count, lineW };
            first = i;
            count = 0;
            lineW = 0;
        }
        lineW += ((count > 0 && L.toks[i].spaceBefore) ? sp : 0) + L.toks[i].w;
        count++;
    }
    if (count > 0 && L.nl < 10) L.lines[L.nl++] = { first, count, lineW };

    L.totalLines = L.nl;
    L.height = L.nl * L.lineH;

    if (L.nl > maxLines) L.nl = maxLines;
}

bool linesFitWidth(const Layout& L, int w) {
    for (int i = 0; i < L.nl; i++) {
        if (L.lines[i].w > w) return false;
    }
    return true;
}

void chooseLayout(const char* rich, const Font& f, int w, int h, int maxLines,
                  Layout& best) {
    for (int px = kHeroMaxPx; px >= kHeroMinPx; px--) {
        layoutAt(rich, f, px, w, maxLines, best);
        if (best.totalLines <= maxLines && best.height <= h &&
            linesFitWidth(best, w)) {
            return;
        }
    }
    layoutAt(rich, f, kHeroMinPx, w, maxLines, best);
}

struct ClipGuard {
    Canvas saved;
    ClipGuard(int x0, int y0, int x1, int y1) {
        saved = g_canvas;
        g_canvas.x0 = imax(g_canvas.x0, x0);
        g_canvas.y0 = imax(g_canvas.y0, y0);
        g_canvas.x1 = imin(g_canvas.x1, x1);
        g_canvas.y1 = imin(g_canvas.y1, y1);
    }
    ~ClipGuard() { g_canvas = saved; }
};

void drawBadge(const Font& f, const Tok& t, int x, int baseline,
               int mainCap, const Palette& pal) {
    const int bpx = t.bpx;
    const int padX = imax(2, bpx / 10);
    const int padY = imax(1, bpx / 14);
    const int cap = capPx(f, bpx);

    const int top = baseline - mainCap - padY - kBadgeBorder;
    const int h = cap + 2 * padY + 2 * kBadgeBorder;

    if (t.kind == TOK_SOLID) {
        fillRectRaw(x, top, t.w, h, pal.solidFill);
        ClipGuard clip(x + kBadgeBorder, top + kBadgeBorder,
                       x + t.w - kBadgeBorder - 1, top + h - kBadgeBorder - 1);
        drawTextN(f, t.s, t.n, x + padX + kBadgeBorder,
                  top + padY + kBadgeBorder + cap, bpx, pal.solidInk, true);
    } else {
        fillRectRaw(x, top, t.w, h, pal.outlineFill);
        strokeRectRaw(x, top, t.w, h, kBadgeBorder, pal.outlineInk);
        ClipGuard clip(x + kBadgeBorder, top + kBadgeBorder,
                       x + t.w - kBadgeBorder - 1, top + h - kBadgeBorder - 1);
        drawTextN(f, t.s, t.n, x + padX + kBadgeBorder,
                  top + padY + kBadgeBorder + cap, bpx, pal.outlineInk, true);
    }
}

void drawLayout(const Layout& L, const Font& f, int x, int y, int w, int h,
                const Palette& pal, bool center) {
    const int sp = spaceW(f, L.px);

    int top = y + (h - L.height) / 2;
    if (top < y) top = y;
    int baseline = top + L.ascent;

    for (int li = 0; li < L.nl; li++) {
        const Line& ln = L.lines[li];
        int pen = center ? (x + (w - ln.w) / 2) : x;
        if (pen < x) pen = x;

        for (int k = 0; k < ln.count; k++) {
            const Tok& t = L.toks[ln.first + k];
            if (k > 0 && t.spaceBefore) pen += sp;

            if (t.kind == TOK_WORD) {
                drawTextN(f, t.s, t.n, pen, baseline, L.px, pal.ink, true);
            } else {
                drawBadge(f, t, pen, baseline, L.cap, pal);
            }
            pen += t.w;
        }
        baseline += L.lineH;
    }
}

struct PlainLine {
    const char* s;
    int n;
    int w;
};

int plainLayout(const Font& f, const char* text, int px, int maxW,
                PlainLine* out, int maxLines, bool upper) {
    const int sp = spaceW(f, px);
    const char* p = text;
    int nl = 0;
    int lineW = 0;
    const char* lineStart = nullptr;
    const char* lineEnd = nullptr;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char* ws = p;
        while (*p && *p != ' ') p++;
        const char* we = p;
        const int ww = widthN(f, ws, (int)(we - ws), px, upper);

        if (lineStart && lineW + sp + ww > maxW) {
            if (nl < maxLines) {
                out[nl++] = { lineStart, (int)(lineEnd - lineStart), lineW };
            }
            lineStart = ws;
            lineEnd = we;
            lineW = ww;
        } else {
            if (!lineStart) {
                lineStart = ws;
                lineW = ww;
            } else {
                lineW += sp + ww;
            }
            lineEnd = we;
        }
    }
    if (lineStart && nl < maxLines) {
        out[nl++] = { lineStart, (int)(lineEnd - lineStart), lineW };
    }
    return nl;
}

int choosePunchPx(const Font& f, const char* text, int maxW, PlainLine* lines,
                  int& nlOut) {
    for (int px = kPunchMaxPx; px >= kPunchMinPx; px--) {
        const int nl = plainLayout(f, text, px, maxW, lines, kPunchMaxLn, true);
        const int h = nl * iround((float)px * 1.20f) + 6;
        if (nl <= kPunchMaxLn && h <= kPunchMaxH) {
            nlOut = nl;
            return px;
        }
    }
    nlOut = plainLayout(f, text, kPunchMinPx, maxW, lines, kPunchMaxLn, true);
    return kPunchMinPx;
}

}
void init() {
    g_hero.ok = stbtt_InitFont(&g_hero.info, unbounded_black_ttf, 0) != 0;
    g_hero.capRatio = 0.75f;
    g_mono.ok = stbtt_InitFont(&g_mono.info, jetbrains_mono_ttf, 0) != 0;
    g_mono.capRatio = 0.73f;
}

void setCanvas(const Canvas& c) { g_canvas = c; }

int drawHero(const char* rich, int x, int y, int w, int h,
             const Palette& pal, bool center, int maxLines) {
    if (!g_hero.ok || !rich) return 0;
    Layout L;
    chooseLayout(rich, g_hero, w, h, maxLines, L);
    drawLayout(L, g_hero, x, y, w, h, pal, center);
    return L.height;
}

int punchHeight(const char* text, int w) {
    if (!g_mono.ok || !text || !*text) return 0;
    const int padX = 8;
    const int padY = 6;
    PlainLine lines[kPunchMaxLn];
    int nl = 0;
    const int px = choosePunchPx(g_mono, text, w - 2 * padX, lines, nl);
    return nl * iround((float)px * 1.20f) + 2 * padY;
}

void drawPunchCard(const char* text, int x, int y, int w,
                   uint16_t fill, uint16_t ink) {
    if (!g_mono.ok || !text || !*text) return;

    const int padX = 8;
    const int padY = 6;

    PlainLine lines[kPunchMaxLn];
    int nl = 0;
    const int px = choosePunchPx(g_mono, text, w - 2 * padX, lines, nl);
    const int lineH = iround((float)px * 1.20f);
    const int h = nl * lineH + 2 * padY;

    fillRectRaw(x, y, w, h, fill);

    const int asc = ascentPx(g_mono, px);
    int baseline = y + padY + asc;
    for (int i = 0; i < nl; i++) {
        const int lx = x + (w - lines[i].w) / 2;
        drawTextN(g_mono, lines[i].s, lines[i].n, lx, baseline, px, ink, true);
        baseline += lineH;
    }
}

void fillRect(int x, int y, int w, int h, uint16_t color) {
    fillRectRaw(x, y, w, h, color);
}

}