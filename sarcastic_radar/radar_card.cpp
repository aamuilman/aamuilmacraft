#include "radar_card.h"

#include "jbtext.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct TypeName {
    const char* code;
    const char* name;
};

static const TypeName kBizjets[] = {
    { "ASTR", "ASTRA SPX" },
    { "B350", "KING AIR 350" },
    { "BE20", "KING AIR 200" },
    { "BE30", "KING AIR 300" },
    { "BE40", "HAWKER 400XP" },
    { "BE9L", "KING AIR 90" },
    { "C25A", "CITATION CJ2" },
    { "C25B", "CITATION CJ3" },
    { "C25C", "CITATION CJ4" },
    { "C25M", "CITATION M2" },
    { "C510", "CITATION MUSTANG" },
    { "C525", "CITATIONJET" },
    { "C550", "CITATION II" },
    { "C560", "CITATION V" },
    { "C56X", "CITATION EXCEL" },
    { "C650", "CITATION III" },
    { "C680", "CITATION SOVEREIGN" },
    { "C68A", "CITATION LATITUDE" },
    { "C700", "CITATION LONGITUDE" },
    { "C750", "CITATION X" },
    { "CL30", "CHALLENGER 300" },
    { "CL35", "CHALLENGER 350" },
    { "CL60", "CHALLENGER 600" },
    { "E35L", "EMBRAER LEGACY 600" },
    { "E50P", "EMBRAER PHENOM 100" },
    { "E545", "EMBRAER LEGACY 450" },
    { "E550", "EMBRAER LEGACY 500" },
    { "E55P", "EMBRAER PHENOM 300" },
    { "EA50", "ECLIPSE 550" },
    { "F2TH", "FALCON 2000" },
    { "F900", "FALCON 900" },
    { "FA20", "FALCON 20" },
    { "FA50", "FALCON 50" },
    { "FA7X", "FALCON 7X" },
    { "FA8X", "FALCON 8X" },
    { "G150", "GULFSTREAM G150" },
    { "G280", "GULFSTREAM G280" },
    { "GA5C", "GULFSTREAM G500" },
    { "GA6C", "GULFSTREAM G600" },
    { "GA7C", "GULFSTREAM G700" },
    { "GA8C", "GULFSTREAM G800" },
    { "GALX", "GULFSTREAM G200" },
    { "GLEX", "GLOBAL EXPRESS" },
    { "GLF2", "GULFSTREAM II" },
    { "GLF3", "GULFSTREAM III" },
    { "GLF4", "GULFSTREAM IV" },
    { "GLF5", "GULFSTREAM V" },
    { "GLF6", "GULFSTREAM G650ER" },
    { "GL5T", "GLOBAL 5000" },
    { "GL6T", "GLOBAL 6000" },
    { "GL7T", "GLOBAL 7500" },
    { "H25A", "HAWKER 700" },
    { "H25B", "HAWKER 800" },
    { "HA4T", "HAWKER 4000" },
    { "HDJT", "HONDAJET" },
    { "LJ31", "LEARJET 31" },
    { "LJ35", "LEARJET 35" },
    { "LJ45", "LEARJET 45" },
    { "LJ55", "LEARJET 55" },
    { "LJ60", "LEARJET 60" },
    { "LJ75", "LEARJET 75" },
    { "P180", "PIAGGIO AVANTI" },
    { "PC12", "PILATUS PC-12" },
    { "PC24", "PILATUS PC-24" },
    { "PRM1", "PREMIER 1A" },
    { "SF50", "CIRRUS VISION JET" },
    { "TBM7", "TBM 700" },
    { "TBM8", "TBM 850" },
    { "TBM9", "TBM 930" },
    { "WW24", "WESTWIND II" },
};

static const TypeName kAirliners[] = {
    { "A19N", "AIRBUS A319NEO" },
    { "A20N", "AIRBUS A320NEO" },
    { "A21N", "AIRBUS A321NEO" },
    { "A306", "AIRBUS A300-600" },
    { "A319", "AIRBUS A319" },
    { "A320", "AIRBUS A320" },
    { "A321", "AIRBUS A321" },
    { "A332", "AIRBUS A330-200" },
    { "A333", "AIRBUS A330-300" },
    { "A339", "AIRBUS A330-900" },
    { "A343", "AIRBUS A340-300" },
    { "A359", "AIRBUS A350-900" },
    { "A35K", "AIRBUS A350-1000" },
    { "A388", "AIRBUS A380" },
    { "AT72", "ATR 72" },
    { "AT76", "ATR 72-600" },
    { "B37M", "BOEING 737 MAX 7" },
    { "B38M", "BOEING 737 MAX 8" },
    { "B39M", "BOEING 737 MAX 9" },
    { "B3XM", "BOEING 737 MAX 10" },
    { "B733", "BOEING 737-300" },
    { "B735", "BOEING 737-500" },
    { "B736", "BOEING 737-600" },
    { "B737", "BOEING 737-700" },
    { "B738", "BOEING 737-800" },
    { "B739", "BOEING 737-900" },
    { "B744", "BOEING 747-400" },
    { "B748", "BOEING 747-8" },
    { "B752", "BOEING 757-200" },
    { "B753", "BOEING 757-300" },
    { "B762", "BOEING 767-200" },
    { "B763", "BOEING 767-300" },
    { "B772", "BOEING 777-200" },
    { "B773", "BOEING 777-300" },
    { "B77L", "BOEING 777-200LR" },
    { "B77W", "BOEING 777-300ER" },
    { "B788", "BOEING 787-8" },
    { "B789", "BOEING 787-9" },
    { "B78X", "BOEING 787-10" },
    { "BCS1", "AIRBUS A220-100" },
    { "BCS3", "AIRBUS A220-300" },
    { "CRJ2", "BOMBARDIER CRJ-200" },
    { "CRJ7", "BOMBARDIER CRJ-700" },
    { "CRJ9", "BOMBARDIER CRJ-900" },
    { "DH8D", "DASH 8 Q400" },
    { "E170", "EMBRAER 170" },
    { "E175", "EMBRAER 175" },
    { "E190", "EMBRAER 190" },
    { "E195", "EMBRAER 195" },
    { "E290", "EMBRAER E190-E2" },
    { "E295", "EMBRAER E195-E2" },
    { "MD82", "MCDONNELL MD-82" },
    { "SU95", "SUKHOI SUPERJET 100" },
};

static bool codeEq(const char* a, const char* b) {
    for (int i = 0; i < 4; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 32);
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 32);
        if (ca != cb) return false;
        if (ca == '\0') return true;
    }
    return true;
}

static const char* lookup(const TypeName* tab, int n, const char* code) {
    if (!code || !code[0]) return nullptr;
    for (int i = 0; i < n; i++) {
        if (codeEq(tab[i].code, code)) return tab[i].name;
    }
    return nullptr;
}

bool cardIsBusinessJet(const char* type) {
    return lookup(kBizjets, (int)(sizeof(kBizjets) / sizeof(kBizjets[0])), type) != nullptr;
}

const char* cardModelName(const char* type) {
    const char* m = lookup(kBizjets, (int)(sizeof(kBizjets) / sizeof(kBizjets[0])), type);
    if (m) return m;
    return lookup(kAirliners, (int)(sizeof(kAirliners) / sizeof(kAirliners[0])), type);
}

int cardFtToM(int ft) {
    if (ft <= 0) return ft < 0 ? -1 : 0;

    return (ft * 3048 + 5000) / 10000;
}

int cardKtToKmh(int kt) {
    if (kt <= 0) return 0;

    return (kt * 1852 + 500) / 1000;
}

void cardKmText(char* out, int cap, float nm) {
    const float km = nm * 1.852f;
    if (km < 10.0f) snprintf(out, cap, "%.1f KM", (double)km);
    else            snprintf(out, cap, "%.0f KM", (double)km);
}

int cardPickIndex(const Target* targets, int count) {
    int   emg = -1, biz = -1, any = -1;
    float emgD = 0.0f, bizD = 0.0f, anyD = 0.0f;

    for (int i = 0; i < count; i++) {
        const Target& t = targets[i];

        if (t.alt_ft < 0) continue;

        if (any < 0 || t.dst_nm < anyD) {
            any  = i;
            anyD = t.dst_nm;
        }
        if (cardIsBusinessJet(t.type) && (biz < 0 || t.dst_nm < bizD)) {
            biz  = i;
            bizD = t.dst_nm;
        }

        if (t.emergency && (emg < 0 || t.dst_nm < emgD)) {
            emg  = i;
            emgD = t.dst_nm;
        }
    }

    if (emg >= 0) return emg;
    return biz >= 0 ? biz : any;
}

CardInfo cardBuild(const Target& t) {
    CardInfo c;
    memset(&c, 0, sizeof(c));
    c.has  = true;

    if (t.emergency)                    c.kind = CardKind::Emergency;
    else if (cardIsBusinessJet(t.type)) c.kind = CardKind::Business;
    else                                c.kind = CardKind::Window;

    const char* model = cardModelName(t.type);
    if (model)            snprintf(c.bort, sizeof(c.bort), "%.*s", (int)sizeof(c.bort) - 1, model);
    else if (t.airline[0]) snprintf(c.bort, sizeof(c.bort), "%.*s", (int)sizeof(c.bort) - 1, t.airline);
    else if (t.type[0])    snprintf(c.bort, sizeof(c.bort), "%.*s", (int)sizeof(c.bort) - 1, t.type);
    else                   snprintf(c.bort, sizeof(c.bort), "UNKNOWN");

    snprintf(c.callsign, sizeof(c.callsign), "%.*s",
             (int)sizeof(c.callsign) - 1,
             t.callsign[0] ? t.callsign : "??????");

    snprintf(c.alt, sizeof(c.alt), "%d M", cardFtToM(t.alt_ft));
    snprintf(c.spd, sizeof(c.spd), "%d KM/H", cardKtToKmh(t.gs_kt));

    c.phrase = cardPhrasePick(c.kind, c.callsign);
    return c;
}

static void setRec(CardRecord& r, const Target& t, const char* value) {
    r.has = true;
    snprintf(r.callsign, sizeof(r.callsign), "%.*s",
             (int)sizeof(r.callsign) - 1,
             t.callsign[0] ? t.callsign : "??????");
    snprintf(r.value, sizeof(r.value), "%s", value);
}

CardRecords cardRecords(const Target* targets, int count) {
    CardRecords r;
    memset(&r, 0, sizeof(r));

    float far = 0.0f, near = 0.0f;
    int   high = 0, fast = 0;

    for (int i = 0; i < count; i++) {
        const Target& t = targets[i];
        if (t.alt_ft < 0) continue;

        char v[17];

        if (!r.far.has || t.dst_nm > far) {
            cardKmText(v, sizeof(v), t.dst_nm);
            setRec(r.far, t, v);
            far = t.dst_nm;
        }
        if (!r.high.has || t.alt_ft > high) {
            snprintf(v, sizeof(v), "%d M", cardFtToM(t.alt_ft));
            setRec(r.high, t, v);
            high = t.alt_ft;
        }

        if (t.gs_kt > 0 && (!r.fast.has || t.gs_kt > fast)) {
            snprintf(v, sizeof(v), "%d KM/H", cardKtToKmh(t.gs_kt));
            setRec(r.fast, t, v);
            fast = t.gs_kt;
        }
        if (!r.near.has || t.dst_nm < near) {
            cardKmText(v, sizeof(v), t.dst_nm);
            setRec(r.near, t, v);
            near = t.dst_nm;
        }
    }
    return r;
}

static const char* kBizPhrases[] = {
    "Urgent shopping trip to Dubai: shoes won't buy themselves.",

    "Champagne is uncorked. CO₂ footprint: +14 tons of pure luxury.",
    "Oysters served at 43,000 ft. Hang in there down below.",
    "A $65M jet flying to Milan just for an espresso.",
    "A Zoom meeting that definitely could have been an email.",
    "The paper straw in your coffee just offset this entire flight.",
    "Wave at the ceiling: that might just be your boss.",
    "Cruising high enough to avoid worldly drama and local taxes.",
};

static const char* kWindowPhrases[] = {
    "180 people up there are choosing chicken or pasta right now.",
    "Wave at the sky: {Callsign} passengers are admiring your clouds.",
    "Somewhere above you, 200 phones are still in Airplane Mode.",
    "They are having tomato juice at 33,000 ft. You are having coffee.",
    "Someone up there is heading on vacation while you stare at a screen.",
    "From up there, your neighbourhood looks like a microchip.",
    "I wonder who lives down in those tiny houses?",
};

static const char* kEmergencyPhrases[] = {
    "Squawking 7700. On the ground below, this is the loudest thing in the sky.",
    "Declaring an emergency on the radio. The sky is being cleared for them.",
    "Rescue is already rolling toward the runway.",
    "{Callsign} is coming down early, and not by choice.",
    "Nothing funny to say about this one. Let them through.",
    "Whatever it is, the crew is flying it down. That is what they train for.",
};

int cardPhraseCount(CardKind k) {
    switch (k) {
        case CardKind::Emergency:
            return (int)(sizeof(kEmergencyPhrases) / sizeof(kEmergencyPhrases[0]));
        case CardKind::Business:
            return (int)(sizeof(kBizPhrases) / sizeof(kBizPhrases[0]));
        case CardKind::Window:
            break;
    }
    return (int)(sizeof(kWindowPhrases) / sizeof(kWindowPhrases[0]));
}

const char* cardPhrase(CardKind k, int idx) {
    const int n = cardPhraseCount(k);
    if (idx < 0 || idx >= n) return "";
    switch (k) {
        case CardKind::Emergency: return kEmergencyPhrases[idx];
        case CardKind::Business:  return kBizPhrases[idx];
        case CardKind::Window:    break;
    }
    return kWindowPhrases[idx];
}

int cardPhrasePick(CardKind k, const char* callsign) {
    const int n = cardPhraseCount(k);
    if (n <= 0) return -1;
    if (!callsign || !callsign[0]) return 0;

    uint32_t h = 2166136261u;
    for (const char* p = callsign; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    return (int)(h % (uint32_t)n);
}

int cardPhraseText(CardKind k, int idx, const char* callsign,
                   char* out, int cap) {
    if (cap <= 0) return 0;
    out[0] = '\0';

    const char* src = cardPhrase(k, idx);
    const char* mark = "{Callsign}";

    const char* at = strstr(src, mark);
    if (!at) {
        snprintf(out, cap, "%s", src);
    } else {
        const char* cs = (callsign && callsign[0]) ? callsign : "the";
        snprintf(out, cap, "%.*s%s%s", (int)(at - src), src, cs, at + strlen(mark));
    }
    return (int)strlen(out);
}

int cardWrap(const char* text, int px, int maxW,
             char out[][CARD_LINE_MAX], int maxLines) {
    if (!text || !text[0] || maxLines <= 0) return 0;

    int lines = 0;
    const char* p = text;

    while (*p && lines < maxLines) {
        while (*p == ' ') p++;
        if (!*p) break;

        char line[CARD_LINE_MAX];
        int  n = 0;
        line[0] = '\0';

        for (;;) {
            while (*p == ' ') p++;
            if (!*p) break;

            const char* w = p;
            while (*p && *p != ' ') p++;
            const int wlen = (int)(p - w);

            const int sep  = (n > 0) ? 1 : 0;
            const int room = CARD_LINE_MAX - 1 - n - sep;

            const bool lineHadText = (n > 0);

            char probe[CARD_LINE_MAX];
            int  take = 0;
            while (take < wlen && take < room) {
                memcpy(probe, line, (size_t)n);
                if (sep) probe[n] = ' ';
                memcpy(probe + n + sep, w, (size_t)take + 1);
                probe[n + sep + take + 1] = '\0';
                if (jb::strW(probe, px) > maxW) break;
                take++;
            }

            if (take < wlen && lineHadText) {
                p = w;
                break;
            }

            if (take == 0) {
                take = 1;
            }

            memcpy(line + n + sep, w, (size_t)take);
            if (sep) line[n] = ' ';
            n += sep + take;
            line[n] = '\0';

            if (take < wlen) {
                p = w + take;
                break;
            }
        }

        if (n == 0) break;
        snprintf(out[lines], CARD_LINE_MAX, "%s", line);
        lines++;
    }

    if (*p && lines > 0) {
        char* last = out[lines - 1];
        const int len = (int)strlen(last);
        if (len >= 3) {
            last[len - 1] = '.';
            last[len - 2] = '.';
            last[len - 3] = '.';
        } else {
            snprintf(last, CARD_LINE_MAX, "...");
        }
    }
    return lines;
}
