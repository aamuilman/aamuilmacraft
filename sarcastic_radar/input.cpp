#include "input.h"
#include "config.h"
#include "led.h"

#include <Arduino.h>
#include <M5Unified.h>

namespace {
QueueHandle_t g_queue = nullptr;

gest::State g_state;

bool g_stable[3] = { false, false, false };
int  g_streak[3] = { 0, 0, 0 };

constexpr uint8_t  PM1_ADDR         = 0x6E;
constexpr uint8_t  PM1_REG_BTN      = 0x48;
constexpr uint8_t  PM1_REG_BTN_CFG1 = 0x49;
constexpr uint8_t  PM1_REG_BTN_CFG2 = 0x4A;
constexpr uint32_t PM1_FREQ         = 100000;

gest::PwrState g_pwr;

bool g_pwrStable = false;
int  g_pwrStreak = 0;

bool readPwrDown(bool& down) {
    uint8_t v = 0;
    if (!M5.In_I2C.readRegister(PM1_ADDR, PM1_REG_BTN, &v, 1, PM1_FREQ)) {
        return false;
    }
    down = (v & 0x01) != 0;
    return true;
}

void powerOffNow(const char* why, uint32_t heldMs) {
    Serial.printf("[power] %s (%u ms) — cutting power\n", why,
                  (unsigned)heldMs);

    ledShutdown();
    vTaskDelay(pdMS_TO_TICKS(LED_STOP_SHOW_MS));

    M5.Power.M5pm1.begin();

    bool off = false;
    for (int i = 0; i < 3 && !off; ++i) {
        off = M5.Power.M5pm1.powerOff();
        if (!off) vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (!off) {
        Serial.println("[power] PMIC refused the shutdown request — "
                       "software power-off failed, the hardware switch is the only way left");
    }

    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void logPmicButtonConfig() {
    uint8_t cfg1 = 0, cfg2 = 0;
    const bool ok1 = M5.In_I2C.readRegister(PM1_ADDR, PM1_REG_BTN_CFG1,
                                            &cfg1, 1, PM1_FREQ);
    const bool ok2 = M5.In_I2C.readRegister(PM1_ADDR, PM1_REG_BTN_CFG2,
                                            &cfg2, 1, PM1_FREQ);
    if (!ok1 || !ok2) {
        Serial.printf("[pwr] BTN_CFG read failed (cfg1=%d cfg2=%d)\n",
                      (int)ok1, (int)ok2);
        return;
    }

    static const uint32_t longMs[4] = { 1000, 2000, 3000, 4000 };
    Serial.printf("[pwr] PMIC: long=%u ms, double-off=%s, single-reset=%s "
                  "(cfg1=0x%02X cfg2=0x%02X)\n",
                  (unsigned)longMs[(cfg1 >> 3) & 0x03],
                  (cfg2 & 0x01) ? "disabled" : "enabled",
                  (cfg1 & 0x01) ? "disabled" : "enabled",
                  cfg1, cfg2);
}

void inputTask(void*) {
    const gest::Timings t = { BTN_LONG_MS, CHORD_MS, SHUTDOWN_HOLD_MS,
                              SHUTDOWN_HOLD_MS };

    uint32_t pwrFails = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(INPUT_PERIOD_MS));

        M5.update();

        const bool raw[3] = { M5.BtnA.isPressed(),
                              M5.BtnB.isPressed(),
                              M5.BtnC.isPressed() };

        bool dn[3];
        for (int i = 0; i < 3; ++i) {
            if (raw[i] == g_stable[i]) {
                g_streak[i] = 0;
            } else if (++g_streak[i] >= INPUT_DEBOUNCE_SAMPLES) {
                g_stable[i] = raw[i];
                g_streak[i] = 0;
            }
            dn[i] = g_stable[i];
        }

        InputEvent ev[4];
        const int n = gest::step(g_state, dn, millis(), t, ev, 4);
        for (int i = 0; i < n; ++i) {
            xQueueSend(g_queue, &ev[i], 0);
        }

        bool pwrRaw = false;
        if (readPwrDown(pwrRaw)) {
            if (pwrFails) {
                Serial.printf("[pwr] PMIC bus came back after %u failed reads\n",
                              (unsigned)pwrFails);
                pwrFails = 0;
            }

            if (pwrRaw == g_pwrStable) {
                g_pwrStreak = 0;
            } else if (++g_pwrStreak >= INPUT_DEBOUNCE_SAMPLES) {
                g_pwrStable = pwrRaw;
                g_pwrStreak = 0;
            }

            InputEvent pev[1];
            const uint32_t now = millis();
            if (gest::stepPwr(g_pwr, g_pwrStable, now, t, pev, 1) > 0) {
                powerOffNow("PWR hold", now - g_pwr.downMs);
            }
        } else if (++pwrFails == 1 || pwrFails % 250 == 0) {
            Serial.printf("[pwr] BTN_STATUS unreadable (%u in a row)\n",
                          (unsigned)pwrFails);
        }
    }
}
}

bool inputBegin() {
    g_queue = xQueueCreate(INPUT_QUEUE_LEN, sizeof(InputEvent));
    if (!g_queue) {
        Serial.printf("[input] could not create a queue of %d events\n",
                      INPUT_QUEUE_LEN);
        return false;
    }

    gest::reset(g_state);
    g_pwr = gest::PwrState{};

    logPmicButtonConfig();

    if (xTaskCreatePinnedToCore(inputTask, "input", 4096, nullptr, 3,
                                nullptr, 1) != pdPASS) {
        Serial.printf("[input] task did not start\n");
        vQueueDelete(g_queue);
        g_queue = nullptr;
        return false;
    }
    return true;
}

bool inputPoll(InputEvent& out) {
    if (!g_queue) return false;
    return xQueueReceive(g_queue, &out, 0) == pdTRUE;
}
