#include "provisioning.h"
#include "config.h"
#include "jbtext.h"
#include "palette.h"
#include "settings.h"
#include "ui.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <M5Unified.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>
#include <stdio.h>
#include <string.h>

namespace {
WebServer g_server(WEB_PORT);
DNSServer g_dns;

bool     g_active        = false;
bool     g_restartPending = false;
uint32_t g_restartAt     = 0;

char g_apSsid[33];
char g_apPass[AP_PASS_LEN + 1];

constexpr int kMaxNets = 12;
String g_nets[kMaxNets];
int    g_netCount  = 0;
bool   g_netsReady = false;

struct FormValues {
    char ssid[33];
    char pass[65];
    char lat[24];
    char lon[24];
    char radius[12];
    char tz[32];
    char metar[5];
};

void copyTo(char* dst, size_t cap, const String& src) {
    strlcpy(dst, src.c_str(), cap);
}

void trim(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';

    size_t skip = 0;
    while (s[skip] == ' ' || s[skip] == '\t') ++skip;
    if (skip > 0) memmove(s, s + skip, n - skip + 1);
}

String htmlEscape(const char* in) {
    String out;
    if (!in) return out;
    for (const char* p = in; *p; ++p) {
        switch (*p) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += *p;       break;
        }
    }
    return out;
}

String qrEscape(const char* in) {
    String out;
    if (!in) return out;
    for (const char* p = in; *p; ++p) {
        if (*p == '\\' || *p == ';' || *p == ',' || *p == ':' || *p == '"') out += '\\';
        out += *p;
    }
    return out;
}

void defaultsFromStored(FormValues& v) {
    v.pass[0] = '\0';

    Settings s;
    if (settings::load(s)) {
        copyTo(v.ssid, sizeof(v.ssid), s.ssid);
        snprintf(v.lat, sizeof(v.lat), "%.4f", s.lat);
        snprintf(v.lon, sizeof(v.lon), "%.4f", s.lon);
        snprintf(v.radius, sizeof(v.radius), "%.0f", (double)s.radiusKm);
        snprintf(v.tz, sizeof(v.tz), "%g", s.utcOffsetMin / 60.0);
        copyTo(v.metar, sizeof(v.metar), s.metar);
        return;
    }

    v.ssid[0] = '\0';
    snprintf(v.lat, sizeof(v.lat), "0");
    snprintf(v.lon, sizeof(v.lon), "0");
    snprintf(v.radius, sizeof(v.radius), "%.0f", (double)RADIUS_KM_DEF);
    snprintf(v.tz, sizeof(v.tz), "0");
    v.metar[0] = '\0';
}

void collectNetworks() {
    if (g_netsReady) return;
    g_netsReady = true;

    const int found = WiFi.scanNetworks();
    if (found <= 0) return;

    for (int i = 0; i < found && g_netCount < kMaxNets; ++i) {
        const String name = WiFi.SSID(i);
        if (name.length() == 0 || name == g_apSsid) continue;

        bool dup = false;
        for (int k = 0; k < g_netCount; ++k) {
            if (g_nets[k] == name) { dup = true; break; }
        }
        if (!dup) g_nets[g_netCount++] = name;
    }
    WiFi.scanDelete();
}

String buildPage(const char* err, const FormValues& v) {
    String p;
    p.reserve(4096);
    p += F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>Sarcastic Radar</title><style>"
           ":root{color-scheme:light dark}"
           "body{font:16px/1.45 system-ui,sans-serif;margin:0;padding:20px;"
           "max-width:560px;margin:0 auto}"
           "h1{font-size:22px;margin:0 0 4px}"
           "p.hint{opacity:.7;font-size:13px}"
           "label{display:block;margin:14px 0 4px;font-size:13px;opacity:.8}"
           "input{width:100%;box-sizing:border-box;padding:10px;font-size:16px;"
           "border:1px solid #8888;border-radius:8px;background:transparent}"
           "button{margin-top:22px;width:100%;padding:14px;font-size:17px;"
           "border:0;border-radius:10px;background:#1f6feb;color:#fff}"
           ".err{padding:10px 12px;border-radius:8px;background:#f8514922;"
           "border:1px solid #f85149;margin:12px 0}"
           "</style></head><body><h1>Sarcastic Radar</h1>"
           "<p class=\"hint\">Location the radar looks around, Wi-Fi and clock."
           " Saved settings survive a reboot.</p>");

    if (err && *err) {
        p += "<div class=\"err\">";
        p += htmlEscape(err);
        p += "</div>";
    }

    p += F("<form method=\"post\" action=\"/save\">");

    p += F("<label for=\"ssid\">Wi-Fi network</label>"
           "<input id=\"ssid\" name=\"ssid\" list=\"nets\" maxlength=\"32\" required value=\"");
    p += htmlEscape(v.ssid);
    p += F("\"><datalist id=\"nets\">");
    for (int i = 0; i < g_netCount; ++i) {
        p += "<option value=\"";
        p += htmlEscape(g_nets[i].c_str());
        p += "\">";
    }
    p += F("</datalist>");

    p += F("<label for=\"pass\">Wi-Fi password</label>"
           "<input id=\"pass\" name=\"pass\" type=\"password\" maxlength=\"63\" value=\"");
    p += htmlEscape(v.pass);
    p += F("\"><p class=\"hint\">Leave it blank to keep the saved password"
           " for this network.</p>");

    p += F("<label for=\"lat\">Latitude</label>"
           "<input id=\"lat\" name=\"lat\" type=\"number\" step=\"any\" min=\"-90\" max=\"90\" required value=\"");
    p += htmlEscape(v.lat);
    p += F("\">");

    p += F("<label for=\"lon\">Longitude</label>"
           "<input id=\"lon\" name=\"lon\" type=\"number\" step=\"any\" min=\"-180\" max=\"180\" required value=\"");
    p += htmlEscape(v.lon);
    p += F("\">");

    p += F("<label for=\"radius\">Radius, km</label>"
           "<select id=\"radius\" name=\"radius\">");

    const float kChoices[2] = { RADIUS_KM_NEAR, RADIUS_KM_FAR };
    for (int i = 0; i < 2; ++i) {
        char value[12];
        snprintf(value, sizeof(value), "%.0f", (double)kChoices[i]);

        p += F("<option value=\"");
        p += value;
        p += "\"";
        if (strcmp(value, v.radius) == 0) p += F(" selected");
        p += ">";
        p += value;
        p += F("</option>");
    }

    p += F("</select>");

    p += F("<label for=\"tz\">Time zone, hours from UTC</label>"
           "<input id=\"tz\" name=\"tz\" type=\"number\" step=\"0.5\" min=\"-12\" max=\"14\" required value=\"");
    p += htmlEscape(v.tz);
    p += F("\">");

    p += F("<label for=\"metar\">METAR station, ICAO (optional)</label>"
           "<input id=\"metar\" name=\"metar\" maxlength=\"4\" placeholder=\"EGLL\" value=\"");
    p += htmlEscape(v.metar);
    p += F("\">");

    p += F("<button type=\"submit\">Save and restart</button></form>"
           "<p class=\"hint\">Long-press the spot on your phone map to read its"
           " coordinates. The METAR station is the airfield whose weather"
           " report appears at the bottom of the radar.</p>"
           "</body></html>");
    return p;
}

void drawText(const char* s, int x, int y, int px, uint16_t col) {
    jb::drawTop(s, x, y, px, col);
}

void drawScreen() {
    LGFX_Sprite& g = ui::canvas();
    g.fillSprite(palette::BG);

    g.drawRect(BEZEL_INSET, BEZEL_INSET, SCREEN_W - 2 * BEZEL_INSET,
               SCREEN_H - 2 * BEZEL_INSET, palette::FIX);

    drawText("SET UP", BEZEL_INSET + 6, 8, 20, palette::DATA);
    drawText("scan the code with your phone camera",
             BEZEL_INSET + 6 + jb::strW("SET UP", 20) + 14, 13, 13, palette::FIX);

    const String payload = String("WIFI:T:WPA;S:") + qrEscape(g_apSsid) +
                           ";P:" + qrEscape(g_apPass) + ";;";
    const int qx = 24;
    const int qy = HEADER_H + 22;
    g.qrcode(payload.c_str(), qx, qy, QR_SIZE_PX, 4, true);

    const int tx = qx + QR_SIZE_PX + 20;
    int y = qy;

    drawText("1. Join the Wi-Fi", tx, y, 15, palette::DATA);
    y += jb::rowH(15) + 6;
    drawText(g_apSsid, tx, y, 19, palette::ALERT);
    y += jb::rowH(19) + 2;
    drawText("password", tx, y, 11, palette::FIX);
    y += jb::rowH(11) + 2;
    drawText(g_apPass, tx, y, 19, palette::DATA);

    y += jb::rowH(19) + 22;
    drawText("2. The setup page opens", tx, y, 15, palette::DATA);
    y += jb::rowH(15) + 6;
    char url[32];
    snprintf(url, sizeof(url), "http://%s", AP_IP);
    drawText(url, tx, y, 19, palette::VIP);
    y += jb::rowH(19) + 10;
    drawText("If it does not, open the address", tx, y, 11, palette::FIX);
    y += jb::rowH(11) + 2;
    drawText("manually in the phone browser.", tx, y, 11, palette::FIX);

    y += jb::rowH(11) + 18;
    drawText("Hold A or C to leave setup", tx, y, 11, palette::FIX);

    ui::setEpdMode(epd_mode_t::epd_fastest);
    ui::present();
}

void scheduleRestart() {
    g_restartPending = true;
    g_restartAt      = millis() + 1500;
}

void handleRoot() {
    if (!g_netsReady) collectNetworks();

    FormValues v;
    defaultsFromStored(v);
    g_server.send(200, "text/html", buildPage(nullptr, v));
}

void handleSave() {
    FormValues v;
    defaultsFromStored(v);

    const String ssidArg  = g_server.arg("ssid");
    const String passArg  = g_server.arg("pass");
    const String metarArg = g_server.arg("metar");

    if (ssidArg.length() >= sizeof(v.ssid) ||
        passArg.length() >= sizeof(v.pass) ||
        metarArg.length() >= sizeof(v.metar)) {
        g_server.send(400, "text/html",
                      buildPage("A field is too long", v));
        return;
    }

    copyTo(v.ssid, sizeof(v.ssid), g_server.arg("ssid"));
    copyTo(v.pass, sizeof(v.pass), g_server.arg("pass"));
    copyTo(v.lat, sizeof(v.lat), g_server.arg("lat"));
    copyTo(v.lon, sizeof(v.lon), g_server.arg("lon"));
    copyTo(v.radius, sizeof(v.radius), g_server.arg("radius"));
    copyTo(v.tz, sizeof(v.tz), g_server.arg("tz"));
    copyTo(v.metar, sizeof(v.metar), g_server.arg("metar"));

    trim(v.ssid);
    trim(v.metar);

    bool passFromStore = false;

    Settings stored;
    if (v.pass[0] == '\0' && settings::load(stored) && strcmp(stored.ssid, v.ssid) == 0) {
        copyTo(v.pass, sizeof(v.pass), stored.pass);
        passFromStore = true;
    }

    for (char* p = v.metar; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    }

    Settings s;
    char err[64] = { 0 };
    if (!settings::parse(v.ssid, v.pass, v.lat, v.lon, v.radius, v.tz, v.metar,
                         s, err, sizeof(err))) {
        if (passFromStore) v.pass[0] = '\0';
        g_server.send(400, "text/html", buildPage(err, v));
        return;
    }

    if (!settings::store(s)) {
        g_server.send(500, "text/html",
                      buildPage("Could not write the settings to flash", v));
        return;
    }

    Serial.printf("[setup] saved: network '%s', %.4f %.4f, %.0f km, UTC%+g, METAR '%s'\n",
                  s.ssid, s.lat, s.lon, (double)s.radiusKm,
                  s.utcOffsetMin / 60.0, s.metar);

    g_server.send(200, "text/html",
                  F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                    "<style>body{font:16px system-ui,sans-serif;padding:40px;text-align:center}</style>"
                    "</head><body><h1>Saved</h1><p>Restarting. The radar will connect"
                    " to the configured network in a moment.</p></body></html>"));
    scheduleRestart();
}

void handleNotFound() {
    g_server.sendHeader("Location", String("http://") + AP_IP + "/", true);
    g_server.send(302, "text/plain", "");
}
}

bool provisioning::begin() {
    if (g_active) return true;

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    const uint64_t mac = ESP.getEfuseMac();

    snprintf(g_apSsid, sizeof(g_apSsid), "%s%02X%02X", AP_PREFIX,
             (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));

    static constexpr char kPassAlphabet[] = "23456789abcdefghjkmnpqrstuvwxyz";
    uint32_t rnd = esp_random();
    for (int i = 0; i < AP_PASS_LEN; ++i) {
        if (i % 8 == 0) rnd = esp_random();
        g_apPass[i] = kPassAlphabet[(rnd >> ((i % 8) * 4)) & 0x1Fu];
    }
    g_apPass[AP_PASS_LEN] = '\0';

    if (!WiFi.softAP(g_apSsid, g_apPass)) {
        Serial.println("[setup] access point did not start");
        return false;
    }

    const IPAddress ip = WiFi.softAPIP();
    Serial.printf("[setup] access point '%s' up at %s\n",
                  g_apSsid, ip.toString().c_str());

    g_dns.setErrorReplyCode(DNSReplyCode::NoError);
    g_dns.start(53, "*", ip);

    g_server.on("/", HTTP_GET, handleRoot);
    g_server.on("/save", HTTP_POST, handleSave);
    g_server.onNotFound(handleNotFound);
    g_server.begin();

    g_active = true;
    drawScreen();
    return true;
}

void provisioning::loop() {
    if (!g_active) return;

    g_dns.processNextRequest();
    g_server.handleClient();

    if (g_restartPending && (int32_t)(millis() - g_restartAt) >= 0) {
        Serial.println("[setup] restarting with the new settings");
        delay(100);
        ESP.restart();
    }
}
