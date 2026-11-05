#ifndef SLANG_ENGINE_H
#define SLANG_ENGINE_H

#include <stdint.h>

namespace slang {

struct DateTime {
    uint16_t year;
    uint8_t  month, day, hour, minute, second;
};

enum Context : uint8_t {
    CTX_NIGHT = 0,
    CTX_MORNING_WORK,
    CTX_DAY_WORK,
    CTX_EVENING_CHILL,
    CTX_FRIDAY_PARTY,
    CTX_WEEKEND,
    CTX_NIGHT_SHIFT,
    CTX_COUNT
};

#define SLANG_MAX_HOLIDAYS 24

enum Audience : uint8_t {
    AUD_MALE = 0,
    AUD_FEMALE = 1,
    AUD_COUNT
};

struct Schedule {
    uint8_t workDays;
    uint8_t startHour, startMinute;
    uint8_t endHour,   endMinute;
    uint8_t holidayCount;
    uint8_t holidays[SLANG_MAX_HOLIDAYS][2];
    uint8_t audience;
};

const Schedule& defaultSchedule();

bool isHoliday(const DateTime& dt, const Schedule& s);
bool isWorkday(const DateTime& dt, const Schedule& s);
bool isWorkTime(const DateTime& dt, const Schedule& s);

void scheduleSummary(const Schedule& s, char* out, unsigned size);

struct History {
    uint32_t seed;
    uint8_t  tmpl;
    uint8_t  punch;
    uint8_t  ctx;
    uint8_t  valid;
};

struct Phrase {
    char     hero[256];
    char     punch[128];
    Context  ctx;
    uint32_t seed;
    uint8_t  slot;
    uint8_t  tmpl;
    uint8_t  punch_index;
};

Context     contextFor(const DateTime& dt);
Context     contextFor(const DateTime& dt, const Schedule& s);
const char* contextLabel(Context c);
const char* audienceLabel(uint8_t audience);

const char* addressFor(uint32_t seed, uint8_t audience);

uint8_t     punchCount(Context c);
const char* punchFor(Context c, uint8_t index, uint8_t audience);
uint8_t     hourIndex(const DateTime& dt);
uint8_t     weekdayMonday1(const DateTime& dt);

uint32_t daysFromCivil(int y, int m, int d);
uint32_t dtToEpoch(const DateTime& dt);

uint32_t seedFor(const DateTime& dt);
uint32_t hash32(uint32_t x);

void generate(const DateTime& dt, const History& prev, Phrase& out, History& next);

void generate(const DateTime& dt, const History& prev, const Schedule& s,
              Phrase& out, History& next);

const char* slotName(uint8_t slot);

}
#endif
