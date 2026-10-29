#include "metar.h"
#include "config.h"
#include "net.h"
#include "tls_root.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include <NetworkClientSecure.h>
#include <string.h>

static constexpr uint32_t METAR_IDLE_MS = 2000;

static void stripProductName(char* s) {
    static const char* const kNames[] = { "METAR ", "SPECI " };

    for (const char* n : kNames) {
        const size_t len = strlen(n);
        if (strncmp(s, n, len) == 0) {
            memmove(s, s + len, strlen(s + len) + 1);
            return;
        }
    }
}

void metarFetch(Metar& m, const char* station) {
    net::assertOwner("metarFetch");

    m = Metar{};

    if (!station || station[0] == '\0') {
        snprintf(m.error, sizeof m.error, "no station");
        return;
    }

    char url[128];
    snprintf(url, sizeof url,
             "https://aviationweather.gov/api/data/metar?ids=%s&format=raw",
             station);

    NetworkClientSecure tls;

    tls.setCACert(kDigiCertRootPem);

    tls.setHandshakeTimeout(HTTP_TIMEOUT_MS / 1000);

    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setReuse(false);

    http.setUserAgent("M5PaperColor-SarcasticRadar/1.0");

    if (!http.begin(tls, url)) {
        snprintf(m.error, sizeof m.error, "begin() failed");
        return;
    }

    m.httpCode = http.GET();
    if (m.httpCode != HTTP_CODE_OK) {
        snprintf(m.error, sizeof m.error, "HTTP %d", m.httpCode);
        http.end();
        return;
    }

    Stream& body = http.getStream();

    const int declared = http.getSize();

    int      n       = 0;
    int      arrived = 0;
    int      lineLen = 0;
    bool     complete = false;
    bool     stalled  = false;
    uint32_t lastByte = millis();
    const uint32_t t0 = lastByte;

    for (;;) {
        if (declared >= 0 && arrived >= declared) { complete = true; break; }
        if (millis() - lastByte > METAR_IDLE_MS) break;
        if (millis() - t0 > METAR_TIMEOUT_MS) { stalled = true; break; }

        const int c = body.read();
        if (c < 0) {
            delay(1);
            continue;
        }
        lastByte = millis();
        arrived++;

        if (c == '\r' || c == '\n') {
            if (n == 0) continue;
            complete = true;
            break;
        }

        lineLen++;

        if (n < METAR_RAW_MAX - 1) m.raw[n++] = (char)c;
    }

    http.end();
    m.raw[n] = '\0';

    if (stalled) {
        snprintf(m.error, sizeof m.error, "read timeout");
        return;
    }

    if (!complete) {
        if (arrived == 0) snprintf(m.error, sizeof m.error, "no body");
        else snprintf(m.error, sizeof m.error, "cut off after %d bytes", arrived);
        return;
    }

    if (lineLen > n) {
        snprintf(m.error, sizeof m.error, "report too long (%d)", lineLen);
        return;
    }

    stripProductName(m.raw);

    if (m.raw[0] == '\0') {
        snprintf(m.error, sizeof m.error, "empty report");
        return;
    }

    m.ok = true;
}
