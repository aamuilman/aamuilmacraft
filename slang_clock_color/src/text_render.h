#ifndef SLANG_TEXT_RENDER_H
#define SLANG_TEXT_RENDER_H

#include <stdint.h>

namespace txt {

struct Palette {
    uint16_t bg;
    uint16_t ink;
    uint16_t solidFill;
    uint16_t solidInk;
    uint16_t outlineFill;
    uint16_t outlineInk;
    uint16_t punchFill;
    uint16_t punchInk;
};

struct Canvas {
    void (*pixel)(int x, int y, uint16_t color);
    int x0, y0, x1, y1;
};

void init();
void setCanvas(const Canvas& c);

int  drawHero(const char* rich, int x, int y, int w, int h,
              const Palette& pal, bool center = true, int maxLines = 6);

int  punchHeight(const char* text, int w);
void drawPunchCard(const char* text, int x, int y, int w,
                   uint16_t fill, uint16_t ink);

void fillRect(int x, int y, int w, int h, uint16_t color);

}
#endif
