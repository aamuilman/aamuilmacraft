#pragma once

#include <stdint.h>
#include <M5GFX.h>

namespace palette {
constexpr uint16_t BG    = WHITE;
constexpr uint16_t DATA  = BLACK;
constexpr uint16_t FIX   = BLUE;
constexpr uint16_t ALERT = RED;

struct PanelColor {
    uint16_t rgb565;
    int      r, g, b;
};

constexpr PanelColor kPanelColors[6] = {
    { 0x0000,   0,   0,   0 },
    { 0xFFFF, 255, 255, 255 },
    { 0xFF87, 255, 243,   0 },
    { 0xB800, 148,   0,   0 },
    { 0x621F,  80, 128, 202 },
    { 0x4443, 102, 133,  68 },
};

constexpr uint16_t PLATE_BLUE   = 0x621F;
constexpr uint16_t PLATE_RED    = 0xB800;
constexpr uint16_t PLATE_YELLOW = 0xFF87;
constexpr uint16_t PLATE_GREEN  = 0x4443;

constexpr uint16_t VIP    = PLATE_BLUE;
constexpr uint16_t ZEN    = PLATE_GREEN;
constexpr uint16_t ACCENT = PLATE_YELLOW;

static_assert(PLATE_BLUE   == kPanelColors[4].rgb565, "PLATE_BLUE does not match kPanelColors[4]");
static_assert(PLATE_RED    == kPanelColors[3].rgb565, "PLATE_RED does not match kPanelColors[3]");
static_assert(PLATE_YELLOW == kPanelColors[2].rgb565, "PLATE_YELLOW does not match kPanelColors[2]");
static_assert(PLATE_GREEN  == kPanelColors[5].rgb565, "PLATE_GREEN does not match kPanelColors[5]");
}
