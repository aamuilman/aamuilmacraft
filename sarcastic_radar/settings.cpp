#include "settings.h"
#include "config.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr const char* kNamespace = "radar";
constexpr uint32_t kVersion = 1;

constexpr const char* kKeyVersion = "ver";
constexpr const char* kKeySsid    = "ssid";
constexpr const char* kKeyPass    = "pass";
constexpr const char* kKeyLat     = "lat";
constexpr const char* kKeyLon     = "lon";
constexpr const char* kKeyRadius  = "rad";
constexpr const char* kKeyUtc     = "utc";
constexpr const char* kKeyMetar   = "metar";

Preferences& nvs() {
    static Preferences p;
    return p;
}

bool copyString(char* dst, size_t cap, const char* src) {
    if (!src) return false;
    const size_t n = strlen(src);
    if (n >= cap) return false;
    memcpy(dst, src, n + 1);
    return true;
}

bool parseNumber(const char* s, double& out) {
    if (!s || *s == '\0') return false;

    char* end = nullptr;
    const double v = strtod(s, &end);
    if (end == s) return false;

    while (*end == ' ' || *end == '\t') ++end;
    if (*end != '\0') return false;
    if (!isfinite(v)) return false;

    out = v;
    return true;
}

void setError(char* err, size_t cap, const char* msg) {
    if (!err || cap == 0) return;
    snprintf(err, cap, "%s", msg);
}
}

bool Settings::valid() const { return settings::validate(*this) == nullptr; }

const char* settings::validate(const Settings& s) {
    const size_t ssidLen = strnlen(s.ssid, sizeof(s.ssid));
    if (ssidLen == 0) return "SSID is empty";
    if (ssidLen >= sizeof(s.ssid)) return "SSID longer than 32 bytes";

    const size_t passLen = strnlen(s.pass, sizeof(s.pass));
    if (passLen > 63) return "password longer than 63 bytes";
    if (passLen > 0 && passLen < 8) return "Wi-Fi password shorter than 8 characters";

    if (!(s.lat >= -90.0 && s.lat <= 90.0)) return "latitude outside -90..90";
    if (!(s.lon >= -180.0 && s.lon <= 180.0)) return "longitude outside -180..180";
    if (s.radiusKm != RADIUS_KM_NEAR && s.radiusKm != RADIUS_KM_FAR) {
        return "radius must be 50 or 100 km";
    }
    if (s.utcOffsetMin < UTC_OFFSET_MIN || s.utcOffsetMin > UTC_OFFSET_MAX) {
        return "time zone outside -12..+14";
    }

    const size_t metarLen = strlen(s.metar);
    if (metarLen != 0 && metarLen != METAR_ID_LEN) return "METAR code must be exactly 4 characters";
    for (const char* p = s.metar; *p; ++p) {
        const bool ok = (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9');
        if (!ok) return "METAR code: uppercase A-Z and digits only";
    }

    return nullptr;
}

bool settings::load(Settings& out) {
    out = Settings{};

    Preferences& p = nvs();
    if (!p.begin(kNamespace, true)) return false;

    const uint32_t ver = p.getUInt(kKeyVersion, 0);
    if (ver != kVersion) {
        p.end();
        return false;
    }

    p.getString(kKeySsid, out.ssid, sizeof(out.ssid));
    p.getString(kKeyPass, out.pass, sizeof(out.pass));
    p.getString(kKeyMetar, out.metar, sizeof(out.metar));
    out.lat          = p.getDouble(kKeyLat, 0.0);
    out.lon          = p.getDouble(kKeyLon, 0.0);
    const float storedRadius = p.getFloat(kKeyRadius, RADIUS_KM_DEF);
    out.radiusKm = (storedRadius >= (RADIUS_KM_NEAR + RADIUS_KM_FAR) / 2.0f)
                       ? RADIUS_KM_FAR
                       : RADIUS_KM_NEAR;
    out.utcOffsetMin = (int)p.getInt(kKeyUtc, 0);
    p.end();

    return validate(out) == nullptr;
}

bool settings::store(const Settings& s) {
    if (validate(s) != nullptr) return false;

    Preferences& p = nvs();
    if (!p.begin(kNamespace, false)) return false;

    const size_t ssidWritten  = p.putString(kKeySsid, s.ssid);
    const size_t passWritten  = p.putString(kKeyPass, s.pass);
    const size_t metarWritten = p.putString(kKeyMetar, s.metar);
    const size_t latWritten   = p.putDouble(kKeyLat, s.lat);
    const size_t lonWritten   = p.putDouble(kKeyLon, s.lon);
    const size_t radWritten   = p.putFloat(kKeyRadius, s.radiusKm);
    const size_t utcWritten   = p.putInt(kKeyUtc, s.utcOffsetMin);
    const size_t verWritten   = p.putUInt(kKeyVersion, kVersion);

    char ssidBack[33] = { 0 };
    char metarBack[5] = { 0 };
    p.getString(kKeySsid, ssidBack, sizeof(ssidBack));
    p.getString(kKeyMetar, metarBack, sizeof(metarBack));
    p.end();

    const bool ok = verWritten == sizeof(uint32_t)
                 && ssidWritten == strlen(s.ssid)
                 && passWritten == strlen(s.pass)
                 && metarWritten == strlen(s.metar)
                 && latWritten == sizeof(double)
                 && lonWritten == sizeof(double)
                 && radWritten == sizeof(float)
                 && utcWritten == sizeof(int)
                 && strcmp(ssidBack, s.ssid) == 0
                 && strcmp(metarBack, s.metar) == 0;

    return ok;
}

void settings::clear() {
    Preferences& p = nvs();
    if (!p.begin(kNamespace, false)) return;
    p.clear();
    p.end();
}

bool settings::parse(const char* ssid, const char* pass,
                     const char* lat, const char* lon, const char* radius,
                     const char* tzHours, const char* metar,
                     Settings& out, char* err, size_t errLen) {
    out = Settings{};

    if (!copyString(out.ssid, sizeof(out.ssid), ssid ? ssid : "")) {
        setError(err, errLen, "SSID longer than 32 bytes");
        return false;
    }
    if (!copyString(out.pass, sizeof(out.pass), pass ? pass : "")) {
        setError(err, errLen, "Wi-Fi password longer than 63 bytes");
        return false;
    }
    if (!copyString(out.metar, sizeof(out.metar), metar ? metar : "")) {
        setError(err, errLen, "METAR code must be exactly 4 characters");
        return false;
    }

    double latV = 0, lonV = 0, radiusV = 0, tzV = 0;
    if (!parseNumber(lat, latV)) {
        setError(err, errLen, "latitude is not a number");
        return false;
    }
    if (!parseNumber(lon, lonV)) {
        setError(err, errLen, "longitude is not a number");
        return false;
    }
    if (!parseNumber(radius, radiusV)) {
        setError(err, errLen, "radius is not a number");
        return false;
    }
    if (!parseNumber(tzHours, tzV)) {
        setError(err, errLen, "time zone is not a number");
        return false;
    }

    out.lat        = latV;
    out.lon        = lonV;
    out.radiusKm   = (float)radiusV;
    out.utcOffsetMin = (int)lround(tzV * 60.0);

    const char* why = validate(out);
    if (why) {
        setError(err, errLen, why);
        return false;
    }
    return true;
}
