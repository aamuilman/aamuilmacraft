#ifndef SLANG_SCREEN_H
#define SLANG_SCREEN_H

#include <stdint.h>

#include "palette.h"
#include "text_render.h"
#include "../slang_engine.h"

namespace screen {

using Pixel = void (*)(int x, int y, uint16_t color);

struct Data {
    const char* hero;
    const char* punch;
    slang::Context ctx;
};

void setPixel(Pixel p);

void draw(const Data& d);

void drawPowerOff();

}
#endif
