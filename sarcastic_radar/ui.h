#pragma once

#include <stdint.h>
#include <M5GFX.h>

namespace ui {
bool begin();

LGFX_Sprite& canvas();

void setEpdMode(epd_mode_t m);

uint32_t present();

void setOwnerTask(void* taskHandle);

void assertOwner(const char* who);

void logHeap(const char* tag);
}
