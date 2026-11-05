#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"

namespace settings {

namespace {
Data g_data;
bool g_loaded = false;

constexpr uint32_t kMagic   = 0x534C4346UL;
constexpr uint32_t kVersion = 2;
}
Data& data() {
    if (!g_loaded) {
        defaults();
        g_loaded = true;
    }
    return g_data;
}

void defaults() {
    memset(&g_data, 0, sizeof(g_data));
    g_data.magic      = kMagic;
    g_data.version    = kVersion;
    g_data.sched      = slang::defaultSchedule();
    g_data.tzOffsetMin = (int16_t)(TZ_OFFSET_SEC / 60);
    snprintf(g_data.ssid, sizeof(g_data.ssid), "%s", WIFI_SSID);
    snprintf(g_data.pass, sizeof(g_data.pass), "%s", WIFI_PASS);
    g_data.configured = 0;
    g_loaded = true;
}

bool load() {
    defaults();

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return false;

    Data d;
    const size_t n = prefs.getBytesLength("cfg");
    bool ok = (n == sizeof(d)) && (prefs.getBytes("cfg", &d, sizeof(d)) == sizeof(d));
    prefs.end();

    if (!ok || d.magic != kMagic || d.version != kVersion) return false;

    if (d.sched.workDays == 0) d.sched.workDays = slang::defaultSchedule().workDays;
    if (d.sched.startHour > 23 || d.sched.endHour > 23) {
        const uint8_t aud = d.sched.audience;
        d.sched = slang::defaultSchedule();
        d.sched.audience = aud;
    }
    if (d.sched.holidayCount > SLANG_MAX_HOLIDAYS) d.sched.holidayCount = 0;
    if (d.sched.audience >= slang::AUD_COUNT) d.sched.audience = slang::AUD_MALE;
    if (d.tzOffsetMin < -12 * 60 || d.tzOffsetMin > 14 * 60) {
        d.tzOffsetMin = (int16_t)(TZ_OFFSET_SEC / 60);
    }
    d.ssid[sizeof(d.ssid) - 1] = 0;
    d.pass[sizeof(d.pass) - 1] = 0;

    g_data = d;
    return d.configured != 0;
}

bool save() {
    g_data.magic   = kMagic;
    g_data.version = kVersion;

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    const size_t n = prefs.putBytes("cfg", &g_data, sizeof(g_data));
    prefs.end();
    return n == sizeof(g_data);
}

}