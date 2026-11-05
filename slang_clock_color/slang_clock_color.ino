#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <esp_sleep.h>
#include <esp_attr.h>
#include <esp_sntp.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "config.h"
#include "slang_engine.h"
#include "src/palette.h"
#include "src/settings.h"
#include "src/setup_portal.h"
#include "src/text_render.h"
#include "src/screen.h"

static M5Canvas g_canvas(&M5.Display);

struct State {
    uint32_t       magic;
    uint32_t       version;
    uint32_t       bootCount;
    slang::History hist;
    uint32_t       lastNtp;
    uint32_t       lastNtpTry;
};

RTC_NOINIT_ATTR State g_state;

struct Persist {
    uint32_t       magic;
    uint32_t       version;
    slang::History hist;
    uint32_t       lastNtp;
};

static bool     g_usbHost        = false;
static bool     g_setupRequested = false;
static uint8_t  g_nvsCounter     = 0;
static bool     g_ledReady       = false;
static volatile bool g_ntpSynced = false;

static bool  rtcRead(slang::DateTime& dt);
static bool  rtcWrite(const slang::DateTime& dt);
static void  maybeSyncTime(slang::DateTime& dt, bool force);
static void  drawCurrent(const slang::DateTime& dt);
static void  drawNow(const slang::DateTime& fallback);
static void  runSetup(const slang::DateTime& dt);
static void  powerOffNow(const char* why, uint32_t heldMs);
static void  gracefulShutdown();
static void  pmicPowerOff();
static void  startPwrTask();
static void  logPmicButtonConfig();
static void  deepSleepFor(uint32_t ms);
static void  sleepUntilNextQuantum(const slang::DateTime& dt);
static inline bool btnDown(int pin);
static int   readPressGesture(int pin);
static void  readPowerGesture();
static void  ledBegin();
static void  ledSet(uint8_t r, uint8_t g, uint8_t b);
static void  ledOff();
static void  ledBlink(uint8_t r, uint8_t g, uint8_t b, int times,
                      uint16_t onMs, uint16_t offMs = 120);

static void ledBegin() {
    if (!M5.Led.isEnabled()) {
        return;
    }
    M5.Led.setBrightness(LED_BRIGHTNESS);
    M5.Led.setAllColor(0, 0, 0);
    g_ledReady = true;
}

static void ledSet(uint8_t r, uint8_t g, uint8_t b) {
    if (!g_ledReady) return;
    M5.Led.setAllColor(r, g, b);
}

static void ledOff() { ledSet(0, 0, 0); }

static void ledBlink(uint8_t r, uint8_t g, uint8_t b, int times,
                     uint16_t onMs, uint16_t offMs) {
    if (!g_ledReady) return;
    for (int i = 0; i < times; i++) {
        ledSet(r, g, b);
        delay(onMs);
        ledOff();
        delay(offMs);
    }
}

static bool rtcRead(slang::DateTime& dt) {
    m5::rtc_datetime_t t;
    if (!M5.Rtc.getDateTime(&t)) return false;

    slang::DateTime r;
    r.year   = (uint16_t)t.date.year;
    r.month  = (uint8_t) t.date.month;
    r.day    = (uint8_t) t.date.date;
    r.hour   = (uint8_t) t.time.hours;
    r.minute = (uint8_t) t.time.minutes;
    r.second = (uint8_t) t.time.seconds;

    if (r.year < 2025 || r.year > 2099) return false;
    if (r.month < 1 || r.month > 12) return false;
    if (r.day < 1 || r.day > 31) return false;
    if (r.hour > 23 || r.minute > 59 || r.second > 59) return false;

    dt = r;
    return true;
}

static bool rtcWrite(const slang::DateTime& dt) {
    m5::rtc_datetime_t t;
    t.date.year    = (int16_t)dt.year;
    t.date.month   = (int8_t) dt.month;
    t.date.date    = (int8_t) dt.day;
    t.date.weekDay = -1;
    t.time.hours   = (int8_t) dt.hour;
    t.time.minutes = (int8_t) dt.minute;
    t.time.seconds = (int8_t) dt.second;
    M5.Rtc.setDateTime(&t);
    return true;
}

static bool buildTime(slang::DateTime& dt) {
    static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char m[4] = { __DATE__[0], __DATE__[1], __DATE__[2], 0 };
    const char* p = strstr(months, m);
    if (!p) return false;
    dt.month  = (uint8_t)((p - months) / 3) + 1;
    dt.day    = (uint8_t)atoi(__DATE__ + 4);
    dt.year   = (uint16_t)atoi(__DATE__ + 7);
    dt.hour   = (uint8_t)atoi(__TIME__);
    dt.minute = (uint8_t)atoi(__TIME__ + 3);
    dt.second = (uint8_t)atoi(__TIME__ + 6);
    return true;
}

static void epochToLocal(uint32_t utc, slang::DateTime& dt) {
    const int32_t  off   = (int32_t)settings::data().tzOffsetMin * 60;
    const uint32_t local = (uint32_t)((int64_t)utc + off);
    const uint32_t days  = local / 86400UL;
    const uint32_t secs  = local % 86400UL;

    int y = 1970;
    unsigned m = 1, d = 1;
    {
        const int64_t z = (int64_t)days + 719468;
        const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe = (unsigned)(z - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp  = (5 * doy + 2) / 153;
        d = doy - (153 * mp + 2) / 5 + 1;
        m = mp + (mp < 10 ? 3 : -9);
        y = (int)(yoe + era * 400) + (m <= 2);
    }

    dt.year   = (uint16_t)y;
    dt.month  = (uint8_t)m;
    dt.day    = (uint8_t)d;
    dt.hour   = (uint8_t)(secs / 3600UL);
    dt.minute = (uint8_t)((secs % 3600UL) / 60UL);
    dt.second = (uint8_t)(secs % 60UL);
}

static void wifiOff() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(10);
}

static void onNtpSync(struct timeval*) { g_ntpSynced = true; }

static bool ntpQuery(slang::DateTime& dt) {
    static const char* const order[3][3] = {
        { NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3 },
        { NTP_SERVER_2, NTP_SERVER_3, NTP_SERVER_1 },
        { NTP_SERVER_3, NTP_SERVER_1, NTP_SERVER_2 },
    };

    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb(onNtpSync);

    for (int i = 0; i < 3; i++) {
        g_ntpSynced = false;
        configTime(0, 0, order[i][0], order[i][1], order[i][2]);

        const uint32_t t0 = millis();
        while (!g_ntpSynced) {
            if (millis() - t0 > NTP_PER_SERVER_MS) break;
            delay(100);
        }
        if (!g_ntpSynced) {
            continue;
        }

        const time_t utc = time(nullptr);
        if (utc < (time_t)NTP_VALID_EPOCH) {
            continue;
        }
        epochToLocal((uint32_t)utc, dt);
        return true;
    }
    return false;
}

static bool ntpSync(slang::DateTime& dt) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings::data().ssid, settings::data().pass);

    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - t0 > WIFI_TIMEOUT_MS) {
            wifiOff();
            return false;
        }
        delay(100);
    }

    slang::DateTime got;
    const bool ok = ntpQuery(got);
    if (ok) dt = got;
    wifiOff();
    return ok;
}

static void savePersist() {
    Persist p;
    memset(&p, 0, sizeof(p));
    p.magic   = PERSIST_MAGIC;
    p.version = PERSIST_VERSION;
    p.hist    = g_state.hist;
    p.lastNtp = g_state.lastNtp;

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putBytes("p", &p, sizeof(p));
    prefs.end();
}

static bool loadPersist() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return false;

    Persist p;
    const size_t n = prefs.getBytesLength("p");
    if (n != sizeof(p) || prefs.getBytes("p", &p, sizeof(p)) != sizeof(p)) {
        prefs.end();
        return false;
    }
    prefs.end();

    if (p.magic != PERSIST_MAGIC || p.version != PERSIST_VERSION) return false;
    g_state.hist    = p.hist;
    g_state.lastNtp = p.lastNtp;
    return true;
}

static void inkToCanvas(int x, int y, uint16_t color) {
    g_canvas.drawPixel(x, y, color);
}

static bool initCanvas() {
    M5.Display.setRotation(SCREEN_ROTATION);
#if EPD_MODE_FASTEST
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
#else
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
#endif
    screen::setPixel(inkToCanvas);

    g_canvas.setColorDepth(16);
    if (!g_canvas.createSprite(EPD_WIDTH, EPD_HEIGHT)) {
        return false;
    }
    g_canvas.fillSprite(pal::WHITE);
    return true;
}

static void present() {
    if (g_canvas.width() <= 0) return;
    const uint32_t t0 = millis();
    g_canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
}

static void drawCurrent(const slang::DateTime& dt) {
    slang::Phrase ph;
    slang::History next;
    slang::generate(dt, g_state.hist, settings::data().sched, ph, next);
    g_state.hist = next;

    if (++g_nvsCounter >= NVS_FLUSH_EVERY) {
        g_nvsCounter = 0;
        savePersist();
    }

    screen::Data d;
    d.hero  = ph.hero;
    d.punch = ph.punch;
    d.ctx   = ph.ctx;
    screen::draw(d);
    present();

}

static void drawNow(const slang::DateTime& fallback) {
    slang::DateTime dt = fallback;
    if (!rtcRead(dt)) dt = fallback;

    drawCurrent(dt);

    slang::DateTime after;
    if (rtcRead(after) && slang::seedFor(after) != slang::seedFor(dt)) {
        drawCurrent(after);
    }
}

static void shiftRtcBy(int32_t deltaSec) {
    if (deltaSec == 0) return;

    slang::DateTime now;
    if (!rtcRead(now)) return;

    const int64_t localSec = (int64_t)slang::dtToEpoch(now) + deltaSec;
    const int64_t utc      = localSec - (int64_t)settings::data().tzOffsetMin * 60;
    if (utc < 0) return;

    slang::DateTime t;
    epochToLocal((uint32_t)utc, t);
    rtcWrite(t);
}

static void runSetup(const slang::DateTime& dt) {
    g_setupRequested = false;
    ledBlink(255, 220, 0, 3, 200);

    const int16_t oldTz = settings::data().tzOffsetMin;

    const portal::Result r = portal::run(g_canvas, present, SETUP_TIMEOUT_MS);
    ledOff();

    if (r == portal::SAVED) {
        const int16_t newTz = settings::data().tzOffsetMin;
        shiftRtcBy(((int32_t)newTz - (int32_t)oldTz) * 60);

        char sched[64];
        slang::scheduleSummary(settings::data().sched, sched, sizeof(sched));
    }

    slang::DateTime now = dt;
    if (!rtcRead(now)) now = dt;
    drawNow(now);
}

static inline bool btnDown(int pin) { return digitalRead(pin) == LOW; }

static int readPressGesture(int pin) {
    const uint32_t t0 = millis();
    bool longHold = false;

    while (btnDown(pin)) {
        if (!longHold && (millis() - t0) >= BTN_SETUP_HOLD_MS) longHold = true;
        delay(10);
    }
    if (longHold) return 3;
    if ((millis() - t0) < BTN_DEBOUNCE_MS) return 0;
    return 1;
}

static void readPowerGesture() {
    const uint32_t t0 = millis();
    while (btnDown(BTN_B_PIN)) {
        if ((millis() - t0) >= BTN_POWER_HOLD_MS) gracefulShutdown();
        delay(10);
    }
}

static void handleWakeButtons(uint64_t mask) {
    if (mask & (1ULL << BTN_B_PIN)) readPowerGesture();

    int gesture = 0;
    int pin = 0;
    if (mask & (1ULL << BTN_A_PIN))      { pin = BTN_A_PIN; gesture = readPressGesture(pin); }
    else if (mask & (1ULL << BTN_C_PIN)) { pin = BTN_C_PIN; gesture = readPressGesture(pin); }
    else if (mask)                       gesture = 1;

    if (gesture == 3 && pin == BTN_C_PIN) g_setupRequested = true;
}

static void armWakePins() {
    const int pins[3] = { BTN_A_PIN, BTN_B_PIN, BTN_C_PIN };
    uint64_t mask = 0;
    for (int i = 0; i < 3; i++) {
        const gpio_num_t g = (gpio_num_t)pins[i];
        if (rtc_gpio_is_valid_gpio(g)) {
            rtc_gpio_pullup_en(g);
            rtc_gpio_pulldown_dis(g);
            mask |= (1ULL << pins[i]);
        }
    }
    esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
}

static void deepSleepFor(uint32_t ms) {
    if (ms < 1000) ms = 1000;

    ledOff();
    M5.Display.sleep();
    M5.Display.waitDisplay();

    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
    armWakePins();
    esp_deep_sleep_start();
}

static void sleepUntilNextQuantum(const slang::DateTime& dt) {
    int to = (QUANT_MINUTES - (dt.minute % QUANT_MINUTES)) * 60 - dt.second;
    if (to <= 0) to += QUANT_MINUTES * 60;
    deepSleepFor((uint32_t)to * 1000UL + 500UL);
}

static void pmicPowerOff() {
    M5.Power.M5pm1.begin();
    bool off = false;
    for (int i = 0; i < 3 && !off; i++) {
        off = M5.Power.M5pm1.powerOff();
        if (!off) delay(10);
    }

    if (!off) {
        ledOff();
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_deep_sleep_start();
    }
    for (;;) delay(1000);
}

static void gracefulShutdown() {
    ledBlink(255, 0, 0, 3, 200);
    ledSet(255, 0, 0);

    if (g_canvas.width() > 0) {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        screen::drawPowerOff();
        present();
    }
    delay(300);

    const bool busy = M5.Display.displayBusy();
    const uint32_t t0 = millis();
    while (M5.Display.displayBusy() &&
           millis() - t0 < PANEL_BUSY_TIMEOUT_MS) {
        delay(20);
    }
    M5.Display.sleep();
    delay(200);

    pmicPowerOff();
}

static void powerOffNow(const char* why, uint32_t heldMs) {
    ledSet(255, 0, 0);
    pmicPowerOff();
}

static bool readPwrDown(bool& down) {
    uint8_t v = 0;
    if (!M5.In_I2C.readRegister(PM1_ADDR, PM1_REG_BTN, &v, 1, PM1_FREQ)) {
        return false;
    }
    down = (v & 0x01) != 0;
    return true;
}

static void logPmicButtonConfig() {
    uint8_t cfg1 = 0, cfg2 = 0;
    const bool ok1 = M5.In_I2C.readRegister(PM1_ADDR, PM1_REG_BTN_CFG1,
                                            &cfg1, 1, PM1_FREQ);
    const bool ok2 = M5.In_I2C.readRegister(PM1_ADDR, PM1_REG_BTN_CFG2,
                                            &cfg2, 1, PM1_FREQ);
    if (!ok1 || !ok2) {
        return;
    }

    static const uint32_t longMs[4] = { 1000, 2000, 3000, 4000 };
}

static void pwrTask(void*) {
    bool stable = false;
    int  streak = 0;
    bool down = false;
    uint32_t downMs = 0;
    bool fired = false;
    uint32_t fails = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(PWR_POLL_MS));

        bool raw = false;
        if (!readPwrDown(raw)) {
            if (++fails == 1 || fails % 250 == 0) {
            }
            continue;
        }
        if (fails) {
            fails = 0;
        }

        if (raw == stable) {
            streak = 0;
        } else if (++streak >= PWR_DEBOUNCE_SAMPLES) {
            stable = raw;
            streak = 0;
        }

        const uint32_t now = millis();
        if (!stable) {
            down = false;
            fired = false;
        } else if (!down) {
            down = true;
            downMs = now;
            fired = false;
        } else if (!fired && (now - downMs) >= PWR_HOLD_MS) {
            fired = true;
            powerOffNow("PWR hold", now - downMs);
        }
    }
}

static void startPwrTask() {
    if (xTaskCreatePinnedToCore(pwrTask, "pwr", 4096, nullptr, 3,
                                nullptr, 1) != pdPASS) {
    }
}

static void maybeSyncTime(slang::DateTime& dt, bool force) {
    const uint32_t now = slang::dtToEpoch(dt);
    const bool due = force || g_state.lastNtp == 0 || now < g_state.lastNtp ||
                     (now - g_state.lastNtp) >= NTP_INTERVAL_SEC;
    if (!due) return;

    if (!force && g_state.lastNtpTry != 0 && now >= g_state.lastNtpTry &&
        (now - g_state.lastNtpTry) < 3600UL) {
        return;
    }

    g_state.lastNtpTry = now;
    slang::DateTime got;
    if (!ntpSync(got)) return;

    rtcWrite(got);
    delay(10);
    slang::DateTime back;
    if (rtcRead(back)) dt = back;
    else               dt = got;

    g_state.lastNtp = slang::dtToEpoch(dt);
    savePersist();
}

void setup() {
    Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
#endif
    for (int i = 0; i < 150 && !Serial; i++) delay(10);
    g_usbHost = (bool)Serial;

    auto cfg = M5.config();
    cfg.clear_display = false;
    cfg.output_power  = false;
    cfg.pmic_button   = false;

    M5.begin(cfg);


    ledBegin();
    ledBlink(255, 255, 255, 2, 150);

    const bool configured = settings::load();
    {
        char sched[64];
        slang::scheduleSummary(settings::data().sched, sched, sizeof(sched));
    }

    logPmicButtonConfig();
    startPwrTask();

    if (!initCanvas()) {
    }

    const bool cold = (g_state.magic != STATE_MAGIC ||
                       g_state.version != STATE_VERSION);
    if (cold) {
        memset(&g_state, 0, sizeof(g_state));
        g_state.magic   = STATE_MAGIC;
        g_state.version = STATE_VERSION;
        loadPersist();
    } else {
    }
    g_state.magic = STATE_MAGIC;
    g_state.version = STATE_VERSION;
    g_state.bootCount++;

    uint64_t wakeMask = 0;
    const esp_sleep_wakeup_cause_t wake = esp_sleep_get_wakeup_cause();
    if (wake == ESP_SLEEP_WAKEUP_EXT1) {
        wakeMask = esp_sleep_get_ext1_wakeup_status();
        handleWakeButtons(wakeMask);
    } else if (wake == ESP_SLEEP_WAKEUP_TIMER) {
    } else {
    }

    slang::DateTime dt = { 2026, 1, 1, 12, 0, 0 };
    bool timeOk = rtcRead(dt);

    if (!timeOk) {
        maybeSyncTime(dt, true);
        timeOk = rtcRead(dt);
    } else {
        maybeSyncTime(dt, cold);
        if (!rtcRead(dt)) timeOk = false;
    }

    if (!timeOk) {
        slang::DateTime bt;
        if (buildTime(bt)) {
            rtcWrite(bt);
            delay(10);
            timeOk = rtcRead(dt);
        }
    }

    if (!timeOk) {
        dt.year = 2026; dt.month = 1; dt.day = 1;
        dt.hour = 12; dt.minute = 0; dt.second = 0;
    }


    if (g_setupRequested) {
        runSetup(dt);
        if (g_usbHost) return;
    } else {
        drawNow(dt);
    }

    if (g_usbHost) {
        return;
    }

    slang::DateTime after;
    if (rtcRead(after)) dt = after;
    sleepUntilNextQuantum(dt);
}

void loop() {
#if SLANG_USB_STAY_AWAKE
    if (!g_usbHost) return;

    slang::DateTime dt;
    if (!rtcRead(dt)) {
        delay(500);
        return;
    }

    const int secs = (QUANT_MINUTES - (dt.minute % QUANT_MINUTES)) * 60 - dt.second;
    const uint32_t waitMs = (uint32_t)(secs > 0 ? secs : QUANT_MINUTES * 60) * 1000UL + 300UL;

    const uint32_t t0 = millis();
    while ((uint32_t)(millis() - t0) < waitMs) {
        if (btnDown(BTN_B_PIN)) {
            readPowerGesture();
            break;
        }
        if (btnDown(BTN_A_PIN) || btnDown(BTN_C_PIN)) {
            const int pin = btnDown(BTN_A_PIN) ? BTN_A_PIN : BTN_C_PIN;
            const int g = readPressGesture(pin);
            if (g == 3 && pin == BTN_C_PIN) {
                runSetup(dt);
                if (!g_usbHost) return;
            } else if (g == 1) {
                drawNow(dt);
            }
            return;
        }
        delay(20);
    }

    drawNow(dt);
#endif
}
