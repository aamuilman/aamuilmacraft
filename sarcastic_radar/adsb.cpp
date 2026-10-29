#include "adsb.h"
#include "airlines.h"
#include "config.h"
#include "net.h"
#include "tls_root.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static void copyTrimmed(char* dst, size_t dstSize, const char* src) {
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (*src == ' ') src++;
    size_t n = strlen(src);
    while (n > 0 && src[n - 1] == ' ') n--;
    if (n > dstSize - 1) n = dstSize - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static constexpr int UNKNOWN_MAX = 16;

static char g_unknown[UNKNOWN_MAX][4];
static int  g_unknownCount = 0;

static void noteUnknownDesignator(const char* callsign) {
    char code[4];
    memcpy(code, callsign, 3);
    code[3] = '\0';

    for (int i = 0; i < g_unknownCount; i++) {
        if (memcmp(g_unknown[i], code, 4) == 0) return;
    }
    if (g_unknownCount >= UNKNOWN_MAX) return;

    memcpy(g_unknown[g_unknownCount], code, 4);
    g_unknownCount++;
}

static void flushUnknownDesignators() {
    if (g_unknownCount == 0) return;

    Serial.print("[air] not in table:");
    for (int i = 0; i < g_unknownCount; i++) {
        Serial.print(' ');
        Serial.print(g_unknown[i]);
    }

    Serial.print('\n');
    g_unknownCount = 0;
}

static int compareTargets(const void* a, const void* b) {
    const Target* x = static_cast<const Target*>(a);
    const Target* y = static_cast<const Target*>(b);
    if (x->emergency != y->emergency) return x->emergency ? -1 : 1;
    if (x->dst_nm < y->dst_nm) return -1;
    if (x->dst_nm > y->dst_nm) return 1;
    return 0;
}

FetchResult adsbFetch(Target* out, int maxTargets,
                      double lat, double lon, float radiusNm) {
    net::assertOwner("adsbFetch");

    FetchResult res;
    res.ok       = false;
    res.count    = 0;
    res.total    = 0;
    res.httpCode = 0;
    res.error[0] = '\0';

    char url[128];
    snprintf(url, sizeof(url),
             "https://api.adsb.lol/v2/point/%.4f/%.4f/%.0f",
             lat, lon, (double)radiusNm);

    NetworkClientSecure tls;
    tls.setCACert(kLetsEncryptRootPem);
    tls.setHandshakeTimeout(HTTP_TIMEOUT_MS / 1000);

    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setReuse(false);

    http.setUserAgent("M5PaperColor-SarcasticRadar/1.0");

    if (!http.begin(tls, url)) {
        snprintf(res.error, sizeof(res.error), "begin() failed");
        return res;
    }

    res.httpCode = http.GET();
    if (res.httpCode != HTTP_CODE_OK) {
        snprintf(res.error, sizeof(res.error), "HTTP %d", res.httpCode);
        http.end();
        return res;
    }

    JsonDocument filter;
    filter["total"]            = true;
    filter["ac"][0]["flight"]  = true;
    filter["ac"][0]["hex"]     = true;
    filter["ac"][0]["t"]       = true;
    filter["ac"][0]["alt_baro"] = true;
    filter["ac"][0]["gs"]      = true;
    filter["ac"][0]["track"]   = true;
    filter["ac"][0]["dst"]     = true;
    filter["ac"][0]["dir"]     = true;
    filter["ac"][0]["emergency"] = true;

    JsonDocument doc;

    DeserializationError err =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        snprintf(res.error, sizeof(res.error), "JSON: %s", err.c_str());
        return res;
    }

    res.total = doc["total"] | 0u;

    JsonArray ac = doc["ac"].as<JsonArray>();
    int n = 0;
    for (JsonObject a : ac) {
        if (n >= maxTargets) break;

        if (a["dst"].isNull() || a["dir"].isNull()) continue;

        Target& t = out[n];

        copyTrimmed(t.callsign, sizeof(t.callsign), a["flight"] | "");

        t.airline[0]  = '\0';
        t.flightNo[0] = '\0';
        if (looksLikeFlightNumber(t.callsign)) {
            const AirlineInfo ai = airlineResolve(t.callsign);
            if (ai.name) {
                strncpy(t.airline, ai.name, AIRLINE_MAX);
                t.airline[AIRLINE_MAX] = '\0';
            } else {
                noteUnknownDesignator(t.callsign);
            }

            if (ai.iata) {
                snprintf(t.flightNo, sizeof(t.flightNo), "%s%s", ai.iata, t.callsign + 3);
            }
        }

        if (t.callsign[0] == '\0') {
            copyTrimmed(t.callsign, sizeof(t.callsign), a["hex"] | "??????");
        }
        copyTrimmed(t.type, sizeof(t.type), a["t"] | "");

        t.dst_nm    = a["dst"].as<float>();
        t.dir_deg   = a["dir"].as<float>();
        t.track_deg = a["track"].isNull() ? NAN : a["track"].as<float>();
        t.gs_kt     = a["gs"].isNull() ? 0 : (int)(a["gs"].as<float>() + 0.5f);

        JsonVariant ab = a["alt_baro"];
        if (ab.is<const char*>() || ab.isNull()) {
            t.alt_ft = -1;
        } else {
            t.alt_ft = ab.as<int>();
        }

        const char* emg = a["emergency"] | "none";
        t.emergency = (strcmp(emg, "none") != 0);

        n++;
    }

    flushUnknownDesignators();

    qsort(out, n, sizeof(Target), compareTargets);

    res.count = n;
    res.ok    = true;
    return res;
}
