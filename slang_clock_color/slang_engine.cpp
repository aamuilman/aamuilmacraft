#include "slang_engine.h"

#include <stdio.h>
#include <string.h>

namespace slang {
namespace {

const int kQuantMinutes = 5;

const char* const kHour[12] = {
    "TWELVE", "ONE", "TWO", "THREE", "FOUR", "FIVE",
    "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN"
};

const char* const kAddressesMale[] = {
    "HOMIE", "BRO", "MAN", "DUDE", "YO",
    "FAM", "CUZ", "BROTHER", "OLD HEAD", "BOSS"
};
const char* const kAddressesFemale[] = {
    "SIS", "GIRL", "QUEEN", "HOMEGIRL", "LADY",
    "SISTER", "CHICA", "BOSS", "FAM", "YO"
};
const uint8_t kAddressCount = (uint8_t)(sizeof(kAddressesMale) / sizeof(kAddressesMale[0]));
static_assert(sizeof(kAddressesMale) / sizeof(kAddressesMale[0]) ==
              sizeof(kAddressesFemale) / sizeof(kAddressesFemale[0]),
              "address banks must have equal length");

const char* const kSlot00[] = {
    "$BRO, [ {OCLOCK} ] exactly. Clock's judging. {PUNCH}",
    "$BRO, it's [ {OCLOCK} ], dead on. Late anyway. {PUNCH}",
    "Straight up [ {OCLOCK} ], $BRO. No excuses. {PUNCH}",
    "That's [ {OCLOCK} ], $BRO. You're not moving. {PUNCH}",
    "$BRO, [ {OCLOCK} ] on the dot. Still lost? {PUNCH}",
};
const char* const kSlot05[] = {
    "$BRO, [ FIVE ] lousy minutes past [ {HOUR} ]. {PUNCH}",
    "Five [ BLASTED ] minutes past [ {HOUR} ], $BRO. {PUNCH}",
    "[ FIVE ] past [ {HOUR} ], and you're already cooked. {PUNCH}",
    "$BRO, [ FIVE ] cursed minutes past [ {HOUR} ]. {PUNCH}",
};
const char* const kSlot10[] = {
    "$BRO, [ TEN ] blasted minutes past [ {HOUR} ]. {PUNCH}",
    "[ TEN ] past [ {HOUR} ], $BRO, clock's crawling. {PUNCH}",
    "Ten [ LOUSY ] minutes past [ {HOUR} ]. {PUNCH}",
    "$BRO, [ TEN ] minutes into [ {HOUR} ]. Any plans? {PUNCH}",
};
const char* const kSlot15[] = {
    "$BRO, [ QUARTER ] past [ {HOUR} ]. Clock's laughing. {PUNCH}",
    "[ QUARTER ] past [ {HOUR} ], $BRO, time's a scam. {PUNCH}",
    "Fifteen [ BLASTED ] minutes past [ {HOUR} ]. {PUNCH}",
    "$BRO, [ QUARTER ] past [ {HOUR} ]. You're cooked. {PUNCH}",
};
const char* const kSlot20[] = {
    "$BRO, [ TWENTY ] blasted minutes past [ {HOUR} ]. {PUNCH}",
    "[ TWENTY ] past [ {HOUR} ], $BRO. Still nothing. {PUNCH}",
    "Twenty [ LOUSY ] minutes past [ {HOUR} ]. {PUNCH}",
    "$BRO, [ TWENTY ] minutes gone, [ {HOUR} ] dragging. {PUNCH}",
};
const char* const kSlot25[] = {
    "$BRO, [ TWENTY-FIVE ] cursed minutes past [ {HOUR} ]. {PUNCH}",
    "[ TWENTY-FIVE ] past [ {HOUR} ], $BRO. Halfway. {PUNCH}",
    "Twenty-five [ BLASTED ] minutes past [ {HOUR} ]. {PUNCH}",
    "$BRO, [ TWENTY-FIVE ] past [ {HOUR} ], half's coming. {PUNCH}",
};
const char* const kSlot30[] = {
    "$BRO, [ HALF PAST ] [ {HOUR} ]. Half gone. {PUNCH}",
    "[ HALF PAST ] [ {HOUR} ], $BRO, day's half dead. {PUNCH}",
    "Thirty [ BLASTED ] minutes past [ {HOUR} ]. {PUNCH}",
    "$BRO, [ HALF PAST ] [ {HOUR} ]. Time's robbing you. {PUNCH}",
};
const char* const kSlot35[] = {
    "$BRO, [ TWENTY-FIVE ] blasted minutes to [ {HOUR} ]. {PUNCH}",
    "[ TWENTY-FIVE ] to [ {HOUR} ], $BRO. You're not. {PUNCH}",
    "Twenty-five [ LOUSY ] minutes to [ {HOUR} ]. {PUNCH}",
    "$BRO, [ TWENTY-FIVE ] to [ {HOUR} ], almost out. {PUNCH}",
};
const char* const kSlot40[] = {
    "$BRO, [ TWENTY ] cursed minutes to [ {HOUR} ]. {PUNCH}",
    "[ TWENTY ] to [ {HOUR} ], $BRO, clock's laughing. {PUNCH}",
    "Twenty [ BLASTED ] minutes to [ {HOUR} ]. {PUNCH}",
    "$BRO, [ TWENTY ] to [ {HOUR} ], keep it moving. {PUNCH}",
};
const char* const kSlot45[] = {
    "$BRO, [ QUARTER ] to [ {HOUR} ]. Clock's on your neck. {PUNCH}",
    "[ QUARTER ] to [ {HOUR} ], $BRO, get it together. {PUNCH}",
    "Fifteen [ BLASTED ] minutes to [ {HOUR} ]. {PUNCH}",
    "$BRO, [ QUARTER ] to [ {HOUR} ]. No more games. {PUNCH}",
};
const char* const kSlot50[] = {
    "$BRO, [ TEN ] blasted minutes to [ {HOUR} ]. {PUNCH}",
    "[ TEN ] to [ {HOUR} ], $BRO, clock's screaming. {PUNCH}",
    "Ten [ LOUSY ] minutes to [ {HOUR} ]. {PUNCH}",
    "$BRO, [ TEN ] to [ {HOUR} ]. Almost there. Sure. {PUNCH}",
};
const char* const kSlot55[] = {
    "$BRO, [ FIVE ] cursed minutes to [ {HOUR} ]. {PUNCH}",
    "[ FIVE ] to [ {HOUR} ], $BRO, clock's done. {PUNCH}",
    "Five [ BLASTED ] minutes to [ {HOUR} ]. {PUNCH}",
    "$BRO, [ FIVE ] to [ {HOUR} ], wrap it up. {PUNCH}",
};

const char* const* const kSlot[12] = {
    kSlot00, kSlot05, kSlot10, kSlot15, kSlot20, kSlot25,
    kSlot30, kSlot35, kSlot40, kSlot45, kSlot50, kSlot55
};
const uint8_t kSlotCount[12] = { 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };

const char* const kSlotLabel[12] = {
    "00", "05", "10", "15", "20", "25", "30", "35", "40", "45", "50", "55"
};

const char* const kPunchNight[] = {
    "Bed. Now. You're not fooling anyone.",
    "Your sleep schedule is a crime scene.",
    "Sleep. You're embarrassing the block.",
    "Go to bed, you absolute mess.",
    "Eyes burning. Still scrolling. Genius.",
    "Tomorrow you'll be a vegetable. Enjoy.",
    "Shut it down. You'll regret this. Again.",
    "Who are you fighting in the dark, champ?",
};
const char* const kPunchMorning[] = {
    "Get your butt to the grind. Move.",
    "Coffee won't save you. Neither will denial.",
    "You already blew every deadline. Legend.",
    "The alarm screamed. You ignored it. Classic.",
    "Get up. The country needs your nonsense.",
    "Welcome to capitalism. It's not impressed.",
    "Move faster. You're late to everything.",
};
const char* const kPunchDay[] = {
    "Productivity left the chat. You didn't notice.",
    "Forty minutes staring at a wall. Genius.",
    "Time's stretching. Your output isn't.",
    "Pretend to work. The boss isn't fooled.",
    "Tasks won't do themselves. Shocking, right?",
    "That smoke break ended ten minutes ago.",
    "Zone out. It's not like you were working.",
};
const char* const kPunchEvening[] = {
    "Workday's dead. You barely showed up.",
    "Couch and series. You've earned both. Barely.",
    "Tea, exhale, pretend today mattered.",
    "Computer off. Your life is waiting. Allegedly.",
    "Close the tickets. Tomorrow you'll ignore them.",
};
const char* const kPunchFriday[] = {
    "Still sober? On a Friday? Embarrassing.",
    "Friday took over. You didn't.",
    "Close the laptop. The weekend's here. You're late.",
    "No deploys on Friday. You'd break it anyway.",
    "Time to go feral. You've been boring all week.",
};
const char* const kPunchWeekend[] = {
    "Legal chill. You've mastered doing nothing.",
    "Doing nothing? You're a natural.",
    "God-tier procrastination. Respect.",
    "Weekends melt fast. Your plans melted faster.",
    "Don't think about Monday. It's coming anyway.",
};

const char* const kPunchNightShift[] = {
    "Three a.m. and still on the clock. Respect. Sort of.",
    "Night shift. The money's okay. You're not.",
    "Everyone's asleep. You're the only one working. Suspicious.",
    "Working nights. Your body files a complaint at dawn.",
    "Coffee at three a.m. Bold. Wrong, but bold.",
    "The building's empty. So is your weekend. Efficient.",
    "Night shift. Vampires get better hours.",
    "Still here. The vending machine is your only friend.",
};

const char* const kPunchFemNight[] = {
    "Bed. The screen won't love you back.",
    "Your beauty sleep called. It hung up.",
    "Sleep, queen. The group chat will survive.",
    "It's 3 a.m. Even your phone is tired.",
    "Scrolling at this hour? Groundbreaking.",
    "Go to bed. Tomorrow needs you rested.",
    "The dark is not a personality. Sleep.",
    "Who are you texting at 3 a.m.? Be honest.",
};
const char* const kPunchFemMorning[] = {
    "Up. The day won't fix itself.",
    "Coffee first, dignity later. Move.",
    "Alarm went off. You chose violence. Against yourself.",
    "Get up, sleepyhead. Work is calling. Rude of it.",
    "You're late. Own it. Walk fast.",
    "Morning. The world is already annoying.",
    "Five more minutes is a lie you tell daily.",
};
const char* const kPunchFemDay[] = {
    "Focus. The deadline is not a suggestion.",
    "That tab isn't going to close itself.",
    "You've reread that email four times. Bold.",
    "Lunch ended an hour ago. Work didn't start.",
    "Staring at the screen won't finish it.",
    "Your to-do list is judging you. Fairly.",
    "Multitasking: two things done badly. Iconic.",
};
const char* const kPunchFemEvening[] = {
    "Laptop closed. Boundaries restored.",
    "You survived. Barely counts, but it counts.",
    "Wine or tea. Choose. Then actually relax.",
    "The day is done. Stop rehearsing it.",
    "Log off. Your couch misses you.",
};
const char* const kPunchFemFriday[] = {
    "Friday. Your out-of-office is showing.",
    "Plans? Or another night with the series?",
    "Go out. The city won't judge you. Much.",
    "Weekend loading. Patience.",
    "No work talk after six. I'm serious.",
};
const char* const kPunchFemWeekend[] = {
    "Nothing on the schedule. Perfect.",
    "Self-care or bed rot. Both valid.",
    "You've earned this nap. Probably.",
    "Sunday scaries are tomorrow's problem.",
    "Doing nothing, beautifully.",
};
const char* const kPunchFemNightShift[] = {
    "3 a.m. shift. You're the main character tonight.",
    "Night shift. The quiet is doing a lot of work.",
    "Coffee at 3 a.m. A classic. A mistake, but classic.",
    "Everyone's asleep. You're holding the place down.",
    "Night shift. The moon gets better hours.",
    "Still going. Respect. And maybe a nap later.",
    "The building is empty. The vending machine is your colleague.",
    "Night shift. Your pillow misses you.",
};

const char* const* const kPunchBank[2][CTX_COUNT] = {
    { kPunchNight, kPunchMorning, kPunchDay,
      kPunchEvening, kPunchFriday, kPunchWeekend, kPunchNightShift },
    { kPunchFemNight, kPunchFemMorning, kPunchFemDay,
      kPunchFemEvening, kPunchFemFriday, kPunchFemWeekend, kPunchFemNightShift },
};
const uint8_t kPunchCount[CTX_COUNT] = { 8, 7, 7, 5, 5, 5, 8 };

const char* const kContextLabel[CTX_COUNT] = {
    "NIGHT", "MORNING", "WORK", "EVENING", "FRIDAY", "WEEKEND", "NIGHTSHIFT"
};

struct Buf {
    char*  p;
    size_t n;
    size_t cap;
};

void putS(Buf& b, const char* s) {
    if (!s) return;
    while (*s && b.n + 1 < b.cap) b.p[b.n++] = *s++;
    b.p[b.n] = '\0';
}

void putC(Buf& b, char c) {
    if (b.n + 1 < b.cap) b.p[b.n++] = c;
    b.p[b.n] = '\0';
}

void trimRight(char* s) {
    size_t n = strlen(s);
    while (n > 0) {
        const char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            s[--n] = '\0';
        } else {
            break;
        }
    }
}

bool startsWith(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

}
uint32_t daysFromCivil(int y, int m, int d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (uint32_t)((long)era * 146097L + (long)doe - 719468L);
}

uint32_t dtToEpoch(const DateTime& dt) {
    return daysFromCivil(dt.year, dt.month, dt.day) * 86400UL
         + (uint32_t)dt.hour * 3600UL + (uint32_t)dt.minute * 60UL + dt.second;
}

uint8_t weekdayMonday1(const DateTime& dt) {
    return (uint8_t)(((daysFromCivil(dt.year, dt.month, dt.day) + 3UL) % 7UL) + 1UL);
}

uint8_t hourIndex(const DateTime& dt) {
    return (uint8_t)(dt.hour % 12);
}

uint32_t seedFor(const DateTime& dt) {
    const uint32_t quant = (uint32_t)((dt.minute / kQuantMinutes) * kQuantMinutes);
    return (uint32_t)dt.day * 100000UL
         + (uint32_t)dt.hour * 1000UL
         + quant;
}

uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dUL;
    x ^= x >> 15;
    x *= 0x846ca68bUL;
    x ^= x >> 16;
    return x;
}

const Schedule& defaultSchedule() {
    static const Schedule s = {
        0x1F,
        9, 0, 18, 0,
        0, {{0, 0}},
        AUD_MALE
    };
    return s;
}

bool isHoliday(const DateTime& dt, const Schedule& s) {
    const uint8_t n = (s.holidayCount > SLANG_MAX_HOLIDAYS)
                    ? SLANG_MAX_HOLIDAYS : s.holidayCount;
    for (uint8_t i = 0; i < n; i++) {
        if (s.holidays[i][0] == dt.month && s.holidays[i][1] == dt.day) {
            return true;
        }
    }
    return false;
}

bool isWorkday(const DateTime& dt, const Schedule& s) {
    const uint8_t wd = weekdayMonday1(dt);
    if (!((s.workDays >> (wd - 1)) & 1)) return false;
    return !isHoliday(dt, s);
}

bool isWorkTime(const DateTime& dt, const Schedule& s) {
    if (!isWorkday(dt, s)) return false;

    const uint16_t now   = (uint16_t)(dt.hour * 60 + dt.minute);
    const uint16_t start = (uint16_t)(s.startHour * 60 + s.startMinute);
    const uint16_t end   = (uint16_t)(s.endHour * 60 + s.endMinute);

    if (start == end) return false;
    if (start < end)  return now >= start && now < end;
    return now >= start || now < end;
}

void scheduleSummary(const Schedule& s, char* out, unsigned size) {
    if (!out || size < 8) return;

    static const char* const kDay[7] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };

    char days[40] = { 0 };
    unsigned w = 0;
    int runStart = -1;

    for (int i = 0; i <= 7; i++) {
        const bool on = (i < 7) && ((s.workDays >> i) & 1);
        if (on && runStart < 0) runStart = i;
        if (!on && runStart >= 0) {
            const int runEnd = i - 1;
            const int n = w;
            if (runEnd - runStart >= 2) {
                w += (unsigned)snprintf(days + w, sizeof(days) - w, "%s%s-%s",
                                        n ? "," : "", kDay[runStart], kDay[runEnd]);
            } else {
                for (int d = runStart; d <= runEnd && w + 8 < sizeof(days); d++) {
                    w += (unsigned)snprintf(days + w, sizeof(days) - w, "%s%s",
                                            (w ? "," : ""), kDay[d]);
                }
            }
            runStart = -1;
        }
    }
    if (!days[0]) snprintf(days, sizeof(days), "no workdays");

    if (s.startHour == s.endHour) {
        snprintf(out, size, "%s, no hours", days);
    } else {
        snprintf(out, size, "%s %02u:%02u-%02u:%02u", days,
                 (unsigned)s.startHour, (unsigned)s.startMinute,
                 (unsigned)s.endHour,   (unsigned)s.endMinute);
    }
}

Context contextFor(const DateTime& dt) {
    return contextFor(dt, defaultSchedule());
}

Context contextFor(const DateTime& dt, const Schedule& s) {
    const uint8_t wd = weekdayMonday1(dt);
    const uint8_t h  = dt.hour;

    const uint16_t now   = (uint16_t)(dt.hour * 60 + dt.minute);
    const uint16_t start = (uint16_t)(s.startHour * 60 + s.startMinute);
    const uint16_t end   = (uint16_t)(s.endHour * 60 + s.endMinute);

    const bool inWork = isWorkTime(dt, s);
    const bool wrap   = start > end;

    if (h <= 4) return inWork ? CTX_NIGHT_SHIFT : CTX_NIGHT;

    if (!isWorkday(dt, s)) return CTX_WEEKEND;

    if (inWork) return CTX_DAY_WORK;

    if (wrap) return CTX_EVENING_CHILL;

    if (now < start) return CTX_MORNING_WORK;
    return (wd == 5) ? CTX_FRIDAY_PARTY : CTX_EVENING_CHILL;
}

const char* contextLabel(Context c) {
    return (c < CTX_COUNT) ? kContextLabel[c] : "UNKNOWN";
}

const char* audienceLabel(uint8_t audience) {
    return (audience == AUD_FEMALE) ? "F" : "M";
}

const char* addressFor(uint32_t seed, uint8_t audience) {
    const char* const* bank = (audience == AUD_FEMALE) ? kAddressesFemale
                                                       : kAddressesMale;
    return bank[hash32(seed ^ 0xC2B2AE35UL) % kAddressCount];
}

uint8_t punchCount(Context c) {
    return (c < CTX_COUNT) ? kPunchCount[c] : 0;
}

const char* punchFor(Context c, uint8_t index, uint8_t audience) {
    if (c >= CTX_COUNT) return "";
    const uint8_t n = kPunchCount[c];
    if (!n) return "";
    const uint8_t aud = (audience == AUD_FEMALE) ? 1 : 0;
    return kPunchBank[aud][c][index % n];
}

const char* slotName(uint8_t slot) {
    return (slot < 12) ? kSlotLabel[slot] : "??";
}

void generate(const DateTime& dt, const History& prev, Phrase& out, History& next) {
    generate(dt, prev, defaultSchedule(), out, next);
}

void generate(const DateTime& dt, const History& prev, const Schedule& sched,
              Phrase& out, History& next) {
    const uint32_t seed = seedFor(dt);
    const uint8_t  slot = (uint8_t)((dt.minute / kQuantMinutes) % 12);
    const Context  ctx  = contextFor(dt, sched);
    const uint8_t  tcount = kSlotCount[slot];
    const uint8_t  pcount = kPunchCount[ctx];

    uint8_t t = 0;
    uint8_t p = 0;

    if (prev.valid && prev.seed == seed && prev.ctx == (uint8_t)ctx &&
        prev.tmpl < tcount && prev.punch < pcount) {
        t = prev.tmpl;
        p = prev.punch;
    } else {
        const uint32_t h = hash32(seed ^ ((uint32_t)ctx * 0x9E3779B9UL) ^ 0x85EBCA6BUL);

        t = (uint8_t)(h % tcount);
        p = (uint8_t)((h >> 11) % pcount);

        if (prev.valid && prev.seed != seed) {
            if (tcount > 1 && prev.tmpl < tcount && t == prev.tmpl) {
                t = (uint8_t)((t + 1 + (hash32(h) % (tcount - 1))) % tcount);
            }
            if (pcount > 1 && prev.punch < pcount && p == prev.punch) {
                p = (uint8_t)((p + 1 + (hash32(h ^ 0xA5A5A5A5UL) % (pcount - 1))) % pcount);
            }
        }
    }

    const char* tmpl = kSlot[slot][t];
    const char* addr = addressFor(seed, sched.audience);

    const bool  toForm = (slot >= 7);
    const uint8_t hourWordIx = (uint8_t)(((dt.hour + (toForm ? 1 : 0)) % 24) % 12);
    const int   hourForWord = (dt.hour + (toForm ? 1 : 0)) % 24;

    const char* hourWord;
    if (hourForWord == 0)       hourWord = "MIDNIGHT";
    else if (hourForWord == 12) hourWord = "NOON";
    else                        hourWord = kHour[hourWordIx];

    char oclock[24];
    if (dt.hour == 0)       snprintf(oclock, sizeof(oclock), "MIDNIGHT");
    else if (dt.hour == 12) snprintf(oclock, sizeof(oclock), "NOON");
    else                    snprintf(oclock, sizeof(oclock), "%s O'CLOCK", kHour[hourIndex(dt)]);

    Buf hb = { out.hero, 0, sizeof(out.hero) };
    out.hero[0] = '\0';

    const char* s = tmpl;
    while (*s) {
        if (startsWith(s, "$BRO")) {
            putC(hb, '\x01');
            putC(hb, ' ');
            putS(hb, addr);
            putC(hb, ' ');
            putC(hb, '\x02');
            s += 4;
        } else if (startsWith(s, "{HOUR}")) {
            putS(hb, hourWord);
            s += 6;
        } else if (startsWith(s, "{OCLOCK}")) {
            putS(hb, oclock);
            s += 8;
        } else if (startsWith(s, "{PUNCH}")) {
            s += 7;
        } else {
            putC(hb, *s++);
        }
    }
    trimRight(out.hero);

    const uint8_t aud = (sched.audience == AUD_FEMALE) ? 1 : 0;
    strncpy(out.punch, kPunchBank[aud][ctx][p], sizeof(out.punch) - 1);
    out.punch[sizeof(out.punch) - 1] = '\0';
    trimRight(out.punch);

    out.ctx         = ctx;
    out.seed        = seed;
    out.slot        = slot;
    out.tmpl        = t;
    out.punch_index = p;

    next.seed  = seed;
    next.tmpl  = t;
    next.punch = p;
    next.ctx   = (uint8_t)ctx;
    next.valid = 1;
}

}