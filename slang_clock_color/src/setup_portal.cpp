#include "setup_portal.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <M5Unified.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lgfx/utility/lgfx_qrcode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "palette.h"
#include "settings.h"
#include "slang_engine.h"

namespace portal {

namespace {

WebServer  g_server(80);
DNSServer  g_dns;
bool       g_saved = false;

constexpr uint8_t  kQrVersion = 4;
constexpr uint8_t  kQrScale   = 6;
constexpr uint8_t  kQrQuiet   = 4;
constexpr uint16_t kQrBox     = (uint16_t)((kQrVersion * 4 + 17) + 2 * kQrQuiet) * kQrScale;

uint8_t g_qrBuf[512];

void drawQr(M5Canvas& c, const char* text, int left, int top) {
    QRCode qr;
    if (lgfx_qrcode_initText(&qr, g_qrBuf, kQrVersion, ECC_LOW, text) != 0) {
        c.setTextColor(pal::RED, pal::WHITE);
        c.drawString("QR failed", left, top);
        return;
    }

    const int span = (qr.size + 2 * kQrQuiet) * kQrScale;
    c.fillRect(left, top, span, span, pal::WHITE);
    for (int y = 0; y < qr.size; y++) {
        for (int x = 0; x < qr.size; x++) {
            if (!lgfx_qrcode_getModule(&qr, (uint_fast8_t)x, (uint_fast8_t)y)) continue;
            c.fillRect(left + (x + kQrQuiet) * kQrScale,
                       top  + (y + kQrQuiet) * kQrScale,
                       kQrScale, kQrScale, pal::BLACK);
        }
    }
}

void drawSetupScreen(M5Canvas& c) {
    c.fillSprite(pal::WHITE);
    c.setTextDatum(textdatum_t::top_center);

    c.setFont(&fonts::FreeSans24pt7b);
    c.setTextColor(pal::BLACK, pal::WHITE);
    c.drawString("SET UP", EPD_WIDTH / 2, 20);

    c.setFont(&fonts::FreeSans9pt7b);
    c.drawString("scan the QR with your phone camera", EPD_WIDTH / 2, 70);

    const int qrLeft = (EPD_WIDTH - kQrBox) / 2;
    drawQr(c, "WIFI:T:WPA;S:" AP_SSID ";P:" AP_PASS ";;", qrLeft, 100);

    int y = 100 + kQrBox + 18;
    c.setFont(&fonts::FreeSans12pt7b);
    c.drawString(AP_SSID, EPD_WIDTH / 2, y);
    y += 30;
    c.setFont(&fonts::FreeSans9pt7b);
    char line[96];
    snprintf(line, sizeof(line), "password: %s", AP_PASS);
    c.drawString(line, EPD_WIDTH / 2, y);
    y += 24;
    c.drawString("then open http://192.168.4.1", EPD_WIDTH / 2, y);

    char sched[64];
    slang::scheduleSummary(settings::data().sched, sched, sizeof(sched));
    y += 34;
    c.setTextColor(pal::BLUE, pal::WHITE);
    c.drawString(sched, EPD_WIDTH / 2, y);

    y += 26;
    c.setTextColor(pal::BLACK, pal::WHITE);
    char meta[64];
    snprintf(meta, sizeof(meta), "18+ sarcasm  |  audience: %s",
             (settings::data().sched.audience == slang::AUD_FEMALE) ? "woman" : "man");
    c.drawString(meta, EPD_WIDTH / 2, y);

    y += 26;
    c.drawString("hold A to cancel", EPD_WIDTH / 2, y);
}

String pageForm() {
    const settings::Data& d = settings::data();

    String days;
    static const char* const kDay[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    for (int i = 0; i < 7; i++) {
        days += "<label><input type=checkbox name=d value=";
        days += String(i + 1);
        if ((d.sched.workDays >> i) & 1) days += " checked";
        days += "> ";
        days += kDay[i];
        days += "</label>";
    }

    String hol;
    for (uint8_t i = 0; i < d.sched.holidayCount && i < SLANG_MAX_HOLIDAYS; i++) {
        char t[8];
        snprintf(t, sizeof(t), "%02u-%02u", (unsigned)d.sched.holidays[i][0],
                 (unsigned)d.sched.holidays[i][1]);
        if (i) hol += ",";
        hol += t;
    }

    char start[8], end[8];
    snprintf(start, sizeof(start), "%02u:%02u", (unsigned)d.sched.startHour,
             (unsigned)d.sched.startMinute);
    snprintf(end, sizeof(end), "%02u:%02u", (unsigned)d.sched.endHour,
             (unsigned)d.sched.endMinute);

    String p;
    p.reserve(3200);
    p += F("<!doctype html><html lang=en><head><meta charset=utf-8>"
           "<meta name=viewport content='width=device-width,initial-scale=1'>"
           "<title>Slang Clock</title><style>"
           "body{font:16px/1.45 system-ui,sans-serif;margin:0;padding:22px;"
           "background:#111;color:#eee}"
           "h1{font-size:22px;margin:0 0 6px}"
           "p{color:#9a9a9a;margin:0 0 20px;font-size:14px}"
           "label{display:block;margin:16px 0 6px;font-size:13px;color:#b5b5b5;"
           "letter-spacing:.03em;text-transform:uppercase}"
           "input[type=text],input[type=time],input[type=number],"
           "input[type=password],textarea{width:100%;box-sizing:border-box;"
           "padding:11px;border-radius:9px;border:1px solid #333;"
           "background:#1b1b1b;color:#eee;font-size:16px}"
           "textarea{resize:vertical}"
           ".days{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}"
           ".days label{display:flex;align-items:center;gap:6px;margin:0;"
           "padding:9px 12px;border:1px solid #333;border-radius:9px;"
           "text-transform:none;letter-spacing:0;color:#eee;font-size:15px}"
           ".note{border:1px solid #5a1f1f;background:#1e1414;color:#e0b4b4;"
           "border-radius:10px;padding:12px 14px;font-size:14px;margin:0 0 18px}"
           ".note b{color:#ff6b6b}"
           ".adult{margin:0 0 6px;padding:12px 14px;border:1px solid #333;"
           "border-radius:9px;background:#1b1b1b}"
           ".adult label{display:flex;align-items:center;gap:9px;margin:0;"
           "text-transform:none;letter-spacing:0;color:#eee;font-size:15px}"
           "button{width:100%;margin-top:24px;padding:15px;border:0;"
           "border-radius:11px;background:#b80000;color:#fff;font-size:17px;"
           "font-weight:600}"
           "</style></head><body><h1>Slang Clock</h1>"
           "<p>Work days and hours. On days off the watch doesn't joke "
           "about work.</p>"
           "<div class=note><b>18+</b> Contains sarcasm, rude jokes and "
           "teasing. Not intended for children.</div>"
           "<form method=post action=/save>"
           "<div class=adult><label><input type=checkbox name=adult value=1 "
           "required> I am 18 years old or older</label></div>"
           "<label>Who the watch talks to</label><div class=days>"
           "<label><input type=radio name=aud value=0");
    p += (d.sched.audience == slang::AUD_FEMALE) ? "" : " checked";
    p += F("> Man</label>"
           "<label><input type=radio name=aud value=1");
    p += (d.sched.audience == slang::AUD_FEMALE) ? " checked" : "";
    p += F("> Woman</label></div>"
           "<label>Work days</label><div class=days>");
    p += days;
    p += F("</div><label>Shift starts</label><input type=time name=start value=");
    p += start;
    p += F("><label>Shift ends</label><input type=time name=end value=");
    p += end;
    p += F("><label>Holidays, MM-DD separated by commas</label>"
           "<textarea name=hol rows=2 placeholder='01-01,02-23,03-08,05-01'>");
    p += hol;
    p += F("</textarea><label>Wi-Fi for accurate time</label>"
           "<input type=text name=ssid value=");
    p += d.ssid;
    p += F("><label>Wi-Fi password</label><input type=password name=pass value=");
    p += d.pass;
    p += F("><label>Time zone, UTC&plusmn;h</label>"
           "<input type=number name=tz min=-12 max=14 step=1 value=");
    p += String(d.tzOffsetMin / 60);
    p += F("><button>Save</button></form></body></html>");
    return p;
}

bool parseClock(const String& v, uint8_t& h, uint8_t& m) {
    int hh = -1, mm = -1;
    if (sscanf(v.c_str(), "%d:%d", &hh, &mm) != 2) return false;
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;
    h = (uint8_t)hh;
    m = (uint8_t)mm;
    return true;
}

void handleSave() {
    settings::Data& d = settings::data();
    slang::Schedule s = d.sched;

    if (g_server.arg("adult") != "1") {
        g_server.send(400, "text/html; charset=utf-8",
                      F("<meta charset=utf-8><p>You must confirm you are 18+. "
                        "<a href=/>Back</a>"));
        return;
    }

    s.audience = (g_server.arg("aud").toInt() == 1) ? slang::AUD_FEMALE
                                                    : slang::AUD_MALE;

    uint8_t mask = 0;
    for (int i = 0; i < g_server.args(); i++) {
        if (g_server.argName(i) != "d") continue;
        const int day = g_server.arg(i).toInt();
        if (day >= 1 && day <= 7) mask |= (uint8_t)(1u << (day - 1));
    }
    if (mask == 0) {
        g_server.send(400, "text/html; charset=utf-8",
                      F("<meta charset=utf-8><p>Select at least one work day. "
                        "<a href=/>Back</a>"));
        return;
    }
    s.workDays = mask;

    uint8_t h, m;
    if (parseClock(g_server.arg("start"), h, m)) { s.startHour = h; s.startMinute = m; }
    if (parseClock(g_server.arg("end"),   h, m)) { s.endHour   = h; s.endMinute   = m; }
    if (s.startHour == s.endHour && s.startMinute == s.endMinute) {
        g_server.send(400, "text/html; charset=utf-8",
                      F("<meta charset=utf-8><p>Shift start and end are the same. "
                        "<a href=/>Back</a>"));
        return;
    }

    s.holidayCount = 0;
    {
        String hol = g_server.arg("hol");
        char buf[256];
        snprintf(buf, sizeof(buf), "%s", hol.c_str());
        char* save = nullptr;
        for (char* tok = strtok_r(buf, ", \t\r\n;", &save);
             tok && s.holidayCount < SLANG_MAX_HOLIDAYS;
             tok = strtok_r(nullptr, ", \t\r\n;", &save)) {
            int mm = 0, dd = 0;
            if (sscanf(tok, "%d-%d", &mm, &dd) != 2) continue;
            if (mm < 1 || mm > 12 || dd < 1 || dd > 31) continue;
            s.holidays[s.holidayCount][0] = (uint8_t)mm;
            s.holidays[s.holidayCount][1] = (uint8_t)dd;
            s.holidayCount++;
        }
    }

    String ssid = g_server.arg("ssid");
    String pass = g_server.arg("pass");
    if (ssid.length()) {
        snprintf(d.ssid, sizeof(d.ssid), "%s", ssid.c_str());
        snprintf(d.pass, sizeof(d.pass), "%s", pass.c_str());
    }

    const int tz = g_server.arg("tz").toInt();
    d.tzOffsetMin = (int16_t)((tz < -12 ? -12 : (tz > 14 ? 14 : tz)) * 60);

    d.sched = s;
    d.configured = 1;

    if (!settings::save()) {
        g_server.send(500, "text/html; charset=utf-8",
                      F("<meta charset=utf-8><p>Couldn't save the settings. "
                        "Please try again.</p>"));
        return;
    }

    char summary[64];
    slang::scheduleSummary(s, summary, sizeof(summary));
    char html[320];
    snprintf(html, sizeof(html),
             "<meta charset=utf-8><meta name=viewport content='width=device-width'>"
             "<body style='font:17px system-ui;background:#111;color:#eee;padding:24px'>"
             "<h2 style='color:#7ec46b'>Saved</h2><p>%s</p>"
             "<p style='color:#999'>The watch will redraw the frame and get "
             "back to work.</p>",
             summary);
    g_server.send(200, "text/html; charset=utf-8", html);
    g_saved = true;
}

void handleRoot()  { g_server.send(200, "text/html; charset=utf-8", pageForm()); }
void handleCatch() { g_server.sendHeader("Location", "/"); g_server.send(302, "text/plain", ""); }

}
Result run(M5Canvas& canvas, void (*presentFn)(void), uint32_t timeoutMs) {
    g_saved = false;

    drawSetupScreen(canvas);
    if (presentFn) presentFn();

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    const bool ap = WiFi.softAP(AP_SSID, AP_PASS);
    const IPAddress ip = WiFi.softAPIP();

    if (!ap) {
        WiFi.mode(WIFI_OFF);
        return TIMEOUT;
    }

    g_dns.start(53, "*", ip);

    g_server.on("/", HTTP_GET, handleRoot);
    g_server.on("/save", HTTP_POST, handleSave);
    g_server.on("/generate_204", handleCatch);
    g_server.on("/hotspot-detect.html", handleCatch);
    g_server.on("/connecttest.txt", handleCatch);
    g_server.onNotFound(handleCatch);
    g_server.begin();

    const uint32_t t0 = millis();
    uint32_t heldFrom = 0;
    while (!g_saved && (uint32_t)(millis() - t0) < timeoutMs) {
        g_dns.processNextRequest();
        g_server.handleClient();

        if (digitalRead(BTN_A_PIN) == LOW) {
            if (!heldFrom) heldFrom = millis();
            else if ((uint32_t)(millis() - heldFrom) >= BTN_CANCEL_HOLD_MS) break;
        } else {
            heldFrom = 0;
        }
        delay(5);
    }

    if (g_saved) delay(400);

    g_server.stop();
    g_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    return g_saved ? SAVED : TIMEOUT;
}

}