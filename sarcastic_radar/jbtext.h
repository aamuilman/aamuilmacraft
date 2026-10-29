#pragma once

#include <stdint.h>

namespace jb {
void init();
bool ready();

void setPixel(void (*fn)(int x, int y, uint16_t color));

void setBounds(int w, int h);

int adv(int px);
int cap(int px);
int rowH(int px);
int strW(const char* s, int px);

float advF(int px);

void draw(const char* s, int x, int baselineY, int px, uint16_t color);

void drawTop(const char* s, int x, int topY, int px, uint16_t color);
}
