#include "radar_mode.h"
#include "config.h"
#include "device.h"
#include "led.h"
#include "metar.h"
#include "net.h"
#include "radar_ui.h"
#include "ui.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace {
Target      g_targets[MAX_TARGETS];
int         g_count = 0;
RadarStatus g_st;

bool     g_everOk    = false;
uint32_t g_lastOkMs  = 0;
uint32_t g_lastTickMs = 0;
bool     g_needTick  = true;

bool     g_terrainDue     = true;
uint32_t g_terrainFailMs  = 0;

NetResult g_net;
Metar     g_metar;
uint32_t  g_metarMs = 0;

Settings g_cfg;

void paintFrame() {
    RadarStatus st = g_st;

    const DeviceStatus& d = dev::status();
    st.battPct     = d.battPct;
    st.extPower    = d.extPower;
    st.chargeTrend = d.chargeTrend;
    st.timeOk      = d.timeOk;
    memcpy(st.timeStr, d.timeStr, sizeof(st.timeStr));
    memcpy(st.zuluStr, d.zuluStr, sizeof(st.zuluStr));

    st.radiusKm = g_cfg.radiusKm;
    snprintf(st.place, sizeof(st.place), "%s", g_cfg.metar);

    st.metarOk = g_metar.ok && (millis() - g_metarMs) < METAR_STALE_MS;
    if (st.metarOk) memcpy(st.metar, g_metar.raw, sizeof(st.metar));
    else            st.metar[0] = '\0';

    ledBusy(true);
    radarPaint(ui::canvas(), g_targets, st.fetchOk ? g_count : 0, st);
    ui::setEpdMode(epd_mode_t::epd_quality);
    ui::present();
    ledBusy(false);
}
}

void radar::begin(const Settings& cfg) {
    g_cfg = cfg;
    g_needTick = true;
    g_terrainDue = true;
    g_terrainFailMs = 0;
}

bool radar::due() {
    return g_needTick || (millis() - g_lastTickMs) >= REFRESH_INTERVAL_MS;
}

void radar::requestRefresh() {
    g_needTick = true;
}

void radar::onButton(BtnId btn, Gesture g) {
    if (btn == BtnId::A && g == Gesture::Short) {
        Serial.println("[btn] BtnA — refreshing now");
        requestRefresh();
    }
}

void radar::tick() {
    g_needTick   = false;
    g_lastTickMs = millis();

    g_st.fetchOk = false;
    g_st.wifiOk  = false;
    g_st.total   = 0;
    g_st.error[0] = '\0';

    dev::samplePower();
    dev::refreshClock();

    const bool wantMetar = g_cfg.metar[0] &&
                           (!g_metar.ok || (millis() - g_metarMs) >= METAR_REFRESH_MS);

    const bool wantTerrain = g_terrainDue &&
                             (!g_terrainFailMs ||
                              (millis() - g_terrainFailMs) >= TERRAIN_RETRY_MS);

    net::fetchTargets(g_targets, MAX_TARGETS, g_net, wantMetar, wantTerrain);
    g_st.wifiOk = g_net.wifiOk;

    if (wantTerrain && g_net.answered) {
        if (g_net.terrain.ok) {
            g_terrainDue    = false;
            g_terrainFailMs = 0;
        } else {
            g_terrainFailMs = millis();
        }
    }

    if (wantMetar && g_net.answered && g_net.metar.ok) {
        g_metar   = g_net.metar;
        g_metarMs = millis();
    }

    if (!g_net.answered) {
        snprintf(g_st.error, sizeof(g_st.error), "%s", g_net.fetch.error);
        Serial.printf("[cycle] %s\n", g_st.error);
    } else if (!g_st.wifiOk) {
        snprintf(g_st.error, sizeof(g_st.error), "Wi-Fi connect failed");
        Serial.println("[cycle] Wi-Fi did not connect");
    } else {
        dev::applyTimeSync(g_net.ntp);
        dev::refreshClock();

        if (g_net.fetch.ok) {
            g_count      = g_net.fetch.count;
            g_st.fetchOk = true;
            g_st.total   = g_net.fetch.total;
            g_everOk     = true;
            g_lastOkMs   = millis();
        } else {
            snprintf(g_st.error, sizeof(g_st.error), "%s", g_net.fetch.error);
        }
    }

    g_st.everOk = g_everOk;
    g_st.ageMin = g_everOk ? (millis() - g_lastOkMs) / 60000u : 0;

    Serial.println("[cycle] radio off, painting");
    paintFrame();
}
