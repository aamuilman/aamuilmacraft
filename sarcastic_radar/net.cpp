#include "net.h"
#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

namespace {
struct Request {
    uint32_t     seq;
    Target*      out;
    int          max;
    bool         wantMetar;
    bool         wantTerrain;
    NetResult*   res;
    TaskHandle_t waiter;
};

QueueHandle_t g_requests    = nullptr;
TaskHandle_t  g_owner       = nullptr;
bool          g_stackLogged = false;
uint32_t      g_seq         = 0;
bool          g_timeSynced  = false;

Settings g_cfg;
bool     g_cfgReady = false;

void wifiDown() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool wifiUp() {
    Serial.printf("[wifi] connecting to '%s'\n", g_cfg.ssid);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);

    if (g_cfg.pass[0]) WiFi.begin(g_cfg.ssid, g_cfg.pass);
    else               WiFi.begin(g_cfg.ssid);

    const uint32_t t0 = millis();
    wl_status_t st = WiFi.status();
    while (millis() - t0 < WIFI_TIMEOUT_MS) {
        st = WiFi.status();
        if (st == WL_CONNECTED) break;
        if (st == WL_NO_SSID_AVAIL) break;
        delay(200);
    }

    const uint32_t took = millis() - t0;
    if (st != WL_CONNECTED) {
        const char* why = (st == WL_NO_SSID_AVAIL) ? "not on the air"
                        : (st == WL_CONNECT_FAILED) ? "heard, but refused"
                        : "link did not come up";
        Serial.printf("[wifi] '%s' failed in %u ms: status %d, %s\n",
                      g_cfg.ssid, (unsigned)took, (int)st, why);
        return false;
    }

    WiFi.setSleep(false);

    Serial.printf("[wifi] '%s' up in %u ms, IP %s, RSSI %d dBm, sleep %d\n",
                  g_cfg.ssid, (unsigned)took, WiFi.localIP().toString().c_str(),
                  (int)WiFi.RSSI(), (int)WiFi.getSleep());
    return true;
}

void syncTimeIfNeeded(dev::NtpResult& out) {
    out = dev::NtpResult{};
    if (g_timeSynced) return;

    out.attempted    = true;
    out.rtcPlausible = time(nullptr) >= TIME_VALID_EPOCH;

    configTime(0, 0, "pool.ntp.org", "time.google.com");

    bool answered = false;
    const uint32_t t0 = millis();
    while (millis() - t0 < NTP_WAIT_MS) {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            answered = true;
            break;
        }
        delay(50);
    }
    out.waitedMs = millis() - t0;
    out.answered = answered;

    g_timeSynced = true;

    const time_t now = time(nullptr);
    gmtime_r(&now, &out.utc);

    if (!answered) {
        Serial.printf("[ntp] SNTP silent for %u ms, time %02d:%02d UTC — %s\n",
                      (unsigned)out.waitedMs, out.utc.tm_hour, out.utc.tm_min,
                      out.rtcPlausible ? "from the battery-backed RTC"
                                       : "RTC empty too, clock is unreliable");
        return;
    }

    Serial.printf("[ntp] SNTP answered in %u ms, time %02d-%02d-%02d %02d:%02d UTC\n",
                  (unsigned)out.waitedMs,
                  out.utc.tm_mday, out.utc.tm_mon + 1, out.utc.tm_year % 100,
                  out.utc.tm_hour, out.utc.tm_min);
}

bool radioUp(bool& wifiOk, bool& radioDown, dev::NtpResult& ntp) {
    wifiOk = wifiUp();
    if (!wifiOk) {
        wifiDown();
        radioDown = true;
        return false;
    }

    syncTimeIfNeeded(ntp);
    return true;
}

void serveFetch(const Request& r) {
    NetResult& res = *r.res;
    res = NetResult{};
    res.seq = r.seq;

    if (!radioUp(res.wifiOk, res.radioDown, res.ntp)) {
        snprintf(res.fetch.error, sizeof(res.fetch.error), "no wifi");
        return;
    }

    const float radiusNm = g_cfg.radiusKm / 1.852f;

    const uint32_t t0 = millis();
    res.fetch = adsbFetch(r.out, r.max, g_cfg.lat, g_cfg.lon, radiusNm);
    const uint32_t dt = millis() - t0;

    if (res.fetch.ok) {
        Serial.printf("[cycle] HTTP %d in %u ms, aircraft %d (of %u), PSRAM free %u\n",
                      res.fetch.httpCode, (unsigned)dt, res.fetch.count,
                      (unsigned)res.fetch.total, (unsigned)ESP.getFreePsram());
    } else {
        Serial.printf("[cycle] error: %s (HTTP %d, %u ms)\n",
                      res.fetch.error, res.fetch.httpCode, (unsigned)dt);
    }

    if (r.wantMetar && g_cfg.metar[0]) {
        const uint32_t t1 = millis();
        metarFetch(res.metar, g_cfg.metar);
        const uint32_t dtm = millis() - t1;

        if (res.metar.ok) {
            Serial.printf("[metar] HTTP %d in %u ms: %s\n",
                          res.metar.httpCode, (unsigned)dtm, res.metar.raw);
        } else {
            Serial.printf("[metar] no report: %s (HTTP %d, %u ms)\n",
                          res.metar.error, res.metar.httpCode, (unsigned)dtm);
        }
    }

    if (r.wantTerrain) {
        terrain::build(g_cfg.lat, g_cfg.lon, g_cfg.radiusKm, res.terrain);
    }

    wifiDown();
    res.radioDown = true;
}

void netTask(void*) {
    g_owner = xTaskGetCurrentTaskHandle();

    for (;;) {
        Request req;
        if (xQueueReceive(g_requests, &req, portMAX_DELAY) != pdTRUE) continue;

        serveFetch(req);

        if (!g_stackLogged) {
            g_stackLogged = true;
            Serial.printf("[net] stack: %u bytes left\n",
                          (unsigned)(uxTaskGetStackHighWaterMark(nullptr) *
                                     sizeof(StackType_t)));
        }

        if (req.res) req.res->answered = true;
        if (req.waiter) xTaskNotifyGive(req.waiter);
    }
}
}

bool net::begin(const Settings& cfg) {
    if (!cfg.valid()) {
        Serial.println("[net] settings incomplete — network not started");
        return false;
    }

    g_cfg      = cfg;
    g_cfgReady = true;

    g_requests = xQueueCreate(NET_QUEUE_LEN, sizeof(Request));
    if (!g_requests) {
        Serial.printf("[net] could not create a request queue of %d\n", NET_QUEUE_LEN);
        return false;
    }

    if (xTaskCreatePinnedToCore(netTask, "net", NET_STACK_BYTES, nullptr,
                                NET_TASK_PRIO, nullptr, NET_TASK_CORE) != pdPASS) {
        Serial.println("[net] task did not start");
        vQueueDelete(g_requests);
        g_requests = nullptr;
        return false;
    }
    return true;
}

void net::shutdown() {
    if (g_requests) {
        ulTaskNotifyTake(pdTRUE, 0);
        xQueueReset(g_requests);
    }
    wifiDown();
}

void net::assertOwner(const char* who) {
    if (!g_owner) return;
    if (xTaskGetCurrentTaskHandle() == g_owner) return;

    Serial.printf("[net] !! %s called from task '%s', but '%s' owns the network\n",
                  who, pcTaskGetName(nullptr), pcTaskGetName(g_owner));
}

void net::fetchTargets(Target* out, int max, NetResult& res, bool wantMetar,
                       bool wantTerrain) {
    res = NetResult{};

    if (!g_cfgReady || !g_requests) {
        snprintf(res.fetch.error, sizeof(res.fetch.error), "net did not start");
        return;
    }

    if (xTaskGetCurrentTaskHandle() == g_owner) {
        Serial.printf("[net] !! fetchTargets called from the network task itself\n");
        snprintf(res.fetch.error, sizeof(res.fetch.error), "called from netTask");
        return;
    }

    const Request req = { ++g_seq, out, max, wantMetar, wantTerrain, &res,
                          xTaskGetCurrentTaskHandle() };

    ulTaskNotifyTake(pdTRUE, 0);

    if (xQueueSend(g_requests, &req, 0) != pdTRUE) {
        Serial.println("[net] !! request queue full — the network task is not answering");
        snprintf(res.fetch.error, sizeof(res.fetch.error), "net queue is full");
        return;
    }

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(NET_REQUEST_TIMEOUT_MS)) == 0) {
        Serial.printf("[net] !! no answer within %u ms\n",
                      (unsigned)NET_REQUEST_TIMEOUT_MS);
        snprintf(res.fetch.error, sizeof(res.fetch.error), "no answer from net");
        return;
    }

    if (res.seq != req.seq) {
        Serial.printf("[net] !! reply for request %u while waiting for %u — dropped\n",
                      (unsigned)res.seq, (unsigned)req.seq);
        res = NetResult{};
        snprintf(res.fetch.error, sizeof(res.fetch.error), "reply to another request");
    }
}
