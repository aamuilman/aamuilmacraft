#include "ui.h"
#include "config.h"
#include "jbtext.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>

namespace {
M5Canvas g_canvas(&M5.Display);

bool     g_ready   = false;
uint32_t g_avgMs   = 0;
bool     g_suspect = false;
void*    g_owner   = nullptr;

void ink(int x, int y, uint16_t color) {
    g_canvas.drawPixel(x, y, color);
}
}

void ui::setOwnerTask(void* taskHandle) { g_owner = taskHandle; }

void ui::assertOwner(const char* who) {
    if (!g_owner) return;
    if (xTaskGetCurrentTaskHandle() == (TaskHandle_t)g_owner) return;

    Serial.printf("[ui] !! %s called from task '%s', but the panel belongs to '%s'\n",
                  who, pcTaskGetName(nullptr), pcTaskGetName((TaskHandle_t)g_owner));
}

bool ui::begin() {
    g_canvas.setColorDepth(16);
    if (!g_canvas.createSprite(SCREEN_W, SCREEN_H)) {
        Serial.printf("[ui] could not create a %dx%d sprite\n", SCREEN_W, SCREEN_H);
        return false;
    }
    g_canvas.setTextWrap(false);
    g_canvas.fillSprite(WHITE);

    jb::init();
    if (!jb::ready()) {
        Serial.println("[ui] font not parsed");
        return false;
    }
    jb::setPixel(ink);
    jb::setBounds(SCREEN_W, SCREEN_H);

    M5.Display.setRotation(SCREEN_ROTATION);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);

    g_ready = true;
    return true;
}

LGFX_Sprite& ui::canvas() { return g_canvas; }

void ui::setEpdMode(epd_mode_t m) {
    assertOwner("setEpdMode");
    M5.Display.setEpdMode(m);
}

uint32_t ui::present() {
    assertOwner("present");
    if (!g_ready) return 0;

    const uint32_t t0 = millis();
    g_canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
    const uint32_t dt = millis() - t0;

    g_suspect = (dt >= PANEL_FRAME_SUSPECT_MS);
    g_avgMs   = g_avgMs ? (g_avgMs * 3 + dt) / 4 : dt;

    Serial.printf("[ui] frame %u ms (avg %u)\n", (unsigned)dt, (unsigned)g_avgMs);
    logHeap("frame");

    if (g_suspect) {
        Serial.printf("[ui] !! frame %u ms exceeds %u ms — the panel wait timed out "
                      "silently, the frame may be incomplete\n",
                      (unsigned)dt, (unsigned)PANEL_FRAME_SUSPECT_MS);
    }
    return dt;
}

void ui::logHeap(const char* tag) {
    Serial.printf("[heap] %-10s int: free %6u min %6u max %6u | PSRAM free %7u\n",
                  tag,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
