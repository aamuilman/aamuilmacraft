#include "device.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <time.h>

#include "config.h"

namespace {
DeviceStatus g_st;

bool g_timeOk = false;

int      g_trend      = 0;
int      g_trendDelta = 0;
int      g_prevMv     = -1;
uint32_t g_prevMvMs   = 0;

int g_battMv = -1;
int g_vbusMv = 0;

int g_utcOffsetMin = 0;

bool     g_vbusState    = false;
int      g_vbusDisagree = 0;
uint32_t g_lastVbusPoll = 0;
bool     g_powerChanged = false;

void printPower() {
    const char* dir = (g_trend > 0) ? "charging"
                    : (g_trend < 0) ? "discharging" : "direction unknown";
    Serial.printf("[pwr] battery %d%% (%d mV), VBUS %d mV, trend %+d mV — %s\n",
                  g_st.battPct, g_battMv, g_vbusMv, g_trendDelta, dir);
}

void updateTrend(int mv) {
    if (mv <= 0) return;

    uint32_t now = millis();
    if (g_prevMv < 0) {
        g_prevMv   = mv;
        g_prevMvMs = now;
        return;
    }
    if (now - g_prevMvMs < BATT_TREND_MIN_MS) return;

    int d = mv - g_prevMv;
    g_trend      = (d > -BATT_TREND_MIN_MV && d < BATT_TREND_MIN_MV) ? 0 : (d > 0 ? 1 : -1);
    g_trendDelta = d;
    g_prevMv     = mv;
    g_prevMvMs   = now;
}

void readPower() {
    g_st.battPct = M5.Power.getBatteryLevel();
    g_battMv     = M5.Power.getBatteryVoltage();

    g_vbusMv     = M5.Power.getVBUSVoltage();
    g_st.extPower = g_vbusMv > VBUS_PRESENT_MV;

    updateTrend(g_battMv);
    g_st.chargeTrend = g_trend;
}
}

void dev::samplePower() {
    readPower();

    g_vbusState    = g_st.extPower;
    g_vbusDisagree = 0;

    g_powerChanged = false;

    printPower();
}

void dev::begin(int utcOffsetMin) {
    g_utcOffsetMin = utcOffsetMin;

    samplePower();
    refreshClock();

    Serial.printf("[dev] time %s / %s, valid: %s\n",
                  g_st.timeStr, g_st.zuluStr, g_st.timeOk ? "yes" : "no");
}

void dev::pollPower() {
    uint32_t now = millis();
    if (now - g_lastVbusPoll < VBUS_POLL_MS) return;
    g_lastVbusPoll = now;

    const bool confirmed = g_st.extPower;
    readPower();
    const bool measured = g_st.extPower;

    g_st.chargeTrend = g_trend;
    if (measured == g_vbusState) {
        g_vbusDisagree = 0;
        return;
    }

    g_st.extPower = confirmed;

    if (++g_vbusDisagree < VBUS_CONFIRM_SAMPLES) return;

    g_vbusDisagree   = 0;
    g_vbusState      = measured;
    g_st.extPower    = measured;
    g_powerChanged   = true;
    printPower();
}

bool dev::takePowerChanged() {
    const bool v = g_powerChanged;
    g_powerChanged = false;
    return v;
}

const DeviceStatus& dev::status() { return g_st; }

void dev::refreshClock() {
    const time_t now = time(nullptr);
    struct tm ut, lt;

    const time_t local = now + (time_t)g_utcOffsetMin * 60;

    if (now < TIME_VALID_EPOCH || !gmtime_r(&now, &ut) || !gmtime_r(&local, &lt)) {
        snprintf(g_st.timeStr, sizeof(g_st.timeStr), "--.--");
        snprintf(g_st.zuluStr, sizeof(g_st.zuluStr), "--.--Z");

        g_st.timeOk = false;
        return;
    }
    snprintf(g_st.timeStr, sizeof(g_st.timeStr), "%02d.%02d", lt.tm_hour, lt.tm_min);
    snprintf(g_st.zuluStr, sizeof(g_st.zuluStr), "%02d.%02dZ", ut.tm_hour, ut.tm_min);
    g_st.timeOk = g_timeOk;
}

void dev::applyTimeSync(const NtpResult& r) {
    if (!r.attempted) return;

    g_timeOk = r.answered || r.rtcPlausible;

    if (!r.answered) return;

    if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&r.utc);
}
