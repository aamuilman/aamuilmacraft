#include "led.h"

#include <Arduino.h>
#include <M5Unified.h>

#include "config.h"

namespace {
enum class Mode : uint8_t { Off, Heartbeat, Shutdown };

SemaphoreHandle_t g_lock = nullptr;

Mode g_mode = Mode::Off;

uint8_t g_lastR = 0xFF;
uint8_t g_lastG = 0xFF;
uint8_t g_lastB = 0xFF;

class Guard {
public:
    Guard()  { if (g_lock) xSemaphoreTake(g_lock, portMAX_DELAY); }
    ~Guard() { if (g_lock) xSemaphoreGive(g_lock); }
};

void paintAll(uint8_t r, uint8_t g, uint8_t b) {
    if (r == g_lastR && g == g_lastG && b == g_lastB) return;

    g_lastR = r;
    g_lastG = g;
    g_lastB = b;

    M5.Led.setColor(0, r, g, b);
    M5.Led.setColor(1, r, g, b);
    M5.Led.display();
}

uint8_t breathe(uint32_t phase) {
    if (phase < LED_BEAT_RISE_MS) {
        return (uint8_t)(255u * phase / LED_BEAT_RISE_MS);
    }

    const uint32_t left = LED_BEAT_RISE_MS + LED_BEAT_FALL_MS - phase;
    if (phase >= LED_BEAT_RISE_MS + LED_BEAT_FALL_MS) return 0;

    return (uint8_t)(255u * left * left /
                     (LED_BEAT_FALL_MS * LED_BEAT_FALL_MS));
}

uint8_t mix(uint8_t channel, uint8_t level) {
    const uint32_t lit = (uint32_t)level * level / 255u;
    return (uint8_t)(lit * channel / 255u);
}

void ledTask(void*) {
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(LED_TICK_MS));

        Guard lock;
        const uint8_t level = breathe(millis() % LED_BEAT_PERIOD_MS);

        switch (g_mode) {
            case Mode::Off:
                paintAll(0, 0, 0);
                break;
            case Mode::Heartbeat:
                paintAll(mix(LED_BEAT_R, level), mix(LED_BEAT_G, level),
                         mix(LED_BEAT_B, level));
                break;
            case Mode::Shutdown:
                paintAll(mix(255, level), 0, 0);
                break;
        }
    }
}
}

void ledBegin() {
    M5.Led.setBrightness(LED_BRIGHT);
    M5.Led.setAutoDisplay(false);

    g_lock = xSemaphoreCreateMutex();

    Guard lock;
    paintAll(0, 0, 0);

    g_mode = Mode::Heartbeat;

    xTaskCreatePinnedToCore(ledTask, "led", 4096, nullptr, 1, nullptr, 1);
}

void ledBusy(bool on) {
    Guard lock;

    if (g_mode == Mode::Shutdown) return;

    g_mode = on ? Mode::Heartbeat : Mode::Off;
}

void ledShutdown() {
    Guard lock;

    g_mode = Mode::Shutdown;
}
