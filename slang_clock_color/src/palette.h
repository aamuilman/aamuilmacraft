#ifndef SLANG_PALETTE_H
#define SLANG_PALETTE_H

#include <stdint.h>

namespace pal {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((uint16_t)(r >> 3) << 11) |
                      ((uint16_t)(g >> 2) << 5)  |
                       (uint16_t)(b >> 3));
}

constexpr uint16_t BLACK  = rgb(  0,   0,   0);
constexpr uint16_t WHITE  = rgb(255, 255, 255);
constexpr uint16_t YELLOW = rgb(255, 243,  56);
constexpr uint16_t RED    = rgb(191,   0,   0);
constexpr uint16_t BLUE   = rgb(100,  64, 255);
constexpr uint16_t GREEN  = rgb( 67, 138,  28);

void     toLinear565(uint16_t p, float& r, float& g, float& b);
uint16_t fromLinear565(float r, float g, float b);

}
#endif
