#include <M5Unified.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "device.h"
#include "input.h"
#include "led.h"
#include "net.h"
#include "provisioning.h"
#include "radar_mode.h"
#include "radar_ui.h"
#include "settings.h"
#include "ui.h"

namespace {
bool g_setupMode = false;

void powerOff() {
    Serial.println("[power] power off by button");

    ledShutdown();

    net::shutdown();

    radarPaintPowerOff(ui::canvas());
    ui::setEpdMode(epd_mode_t::epd_fastest);
    ui::present();
    delay(300);

    const uint32_t t0 = millis();
    while (M5.Display.displayBusy() && millis() - t0 < PANEL_BUSY_TIMEOUT_MS) {
        delay(20);
    }
    Serial.printf("[power] panel %u ms after the frame: %s\n",
                  (unsigned)(millis() - t0),
                  M5.Display.displayBusy() ? "still busy" : "idle");
    M5.Display.sleep();
    delay(200);

    M5.Power.M5pm1.begin();
    bool off = false;
    for (int i = 0; i < 3 && !off; ++i) {
        off = M5.Power.M5pm1.powerOff();
        if (!off) delay(10);
    }
    if (!off) {
        Serial.println("[power] PMIC refused the shutdown request — "
                       "software power-off failed, the hardware switch is the only way left");
    }

    for (;;) delay(1000);
}

void enterSetup() {
    if (g_setupMode) return;

    net::shutdown();

    if (provisioning::begin()) {
        g_setupMode = true;
        ledBusy(false);
    } else {
        Serial.println("[setup] access point did not start");
    }
}

void drainInput() {
    InputEvent ev;
    while (inputPoll(ev)) {
        if (ev.kind == InputEvent::Kind::Sys) {
            if (ev.sys == SysEvent::PowerOff) powerOff();
            if (ev.sys == SysEvent::WifiWindow) enterSetup();
            continue;
        }
        if (ev.kind != InputEvent::Kind::Button) continue;

        radar::onButton(ev.btn, ev.gesture);
    }
}

void armLoopWatchdog() {
    esp_task_wdt_config_t wdt = {};
    wdt.timeout_ms     = LOOP_WDT_MS;
    wdt.idle_core_mask = 1u << 0;
    wdt.trigger_panic  = true;

    esp_err_t e = esp_task_wdt_reconfigure(&wdt);
    if (e == ESP_ERR_INVALID_STATE) e = esp_task_wdt_init(&wdt);

    if (e != ESP_OK) {
        Serial.printf("[wdt] loop watchdog NOT armed (%d)\n", (int)e);
        return;
    }

    enableLoopWDT();
    Serial.printf("[wdt] loop watchdog armed: %u ms\n", (unsigned)LOOP_WDT_MS);
}
}

void setup() {
    Serial.begin(115200);

    const uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 1500) delay(10);

    auto cfg = M5.config();
    cfg.clear_display  = false;
    cfg.pmic_button    = false;
    cfg.output_power   = false;
    M5.begin(cfg);

    Serial.println();
    Serial.println("=== Sarcastic Radar / M5PaperColor ===");
    Serial.printf("board=%d  PSRAM=%u bytes  OPI=%d\n",
                  (int)M5.getBoard(),
                  (unsigned)ESP.getPsramSize(),
                  (int)CONFIG_SPIRAM_MODE_OCT);

    if (!ui::begin()) {
        Serial.println("!! display layer did not start — nothing to draw on");
    }
    ui::setOwnerTask(xTaskGetCurrentTaskHandle());

    if (M5.Display.width() != SCREEN_W || M5.Display.height() != SCREEN_H) {
        Serial.printf("!! display is %dx%d, but the layout expects %dx%d\n",
                      (int)M5.Display.width(), (int)M5.Display.height(),
                      SCREEN_W, SCREEN_H);
    }

    ui::logHeap("boot");

    ledBegin();

    if (!inputBegin()) {
        Serial.println("!! buttons are not being polled");
    }

    Settings s;
    if (settings::load(s)) {
        Serial.printf("[cfg] network '%s', %.4f %.4f, %.0f km, UTC%+g, METAR '%s'\n",
                      s.ssid, s.lat, s.lon, (double)s.radiusKm,
                      s.utcOffsetMin / 60.0, s.metar);

        dev::begin(s.utcOffsetMin);

        if (!net::begin(s)) {
            Serial.println("!! network did not start — frames will carry no data");
        }
        radar::begin(s);
    } else {
        Serial.println("[cfg] no settings — showing the setup QR");
        dev::begin(0);
        enterSetup();
    }

    armLoopWatchdog();
}

void loop() {
    if (g_setupMode) {
        InputEvent ev;
        while (inputPoll(ev)) {
            if (ev.kind == InputEvent::Kind::Sys && ev.sys == SysEvent::PowerOff) {
                powerOff();
            }
            if (ev.kind == InputEvent::Kind::Button && ev.gesture == Gesture::Long) {
                Serial.println("[setup] leaving setup mode");
                delay(50);
                ESP.restart();
            }
        }

        provisioning::loop();
        delay(20);
        return;
    }

    drainInput();

    if (g_setupMode) {
        provisioning::loop();
        delay(20);
        return;
    }

    dev::pollPower();
    if (dev::takePowerChanged()) radar::requestRefresh();

    if (radar::due()) radar::tick();

    delay(50);
}
