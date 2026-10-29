#include "airlines.h"

#include <stdlib.h>
#include <string.h>

#include "airlines_table.h"

static int compareCode(const void* key, const void* elem) {
    const char*         code = static_cast<const char*>(key);
    const AirlineEntry* e    = static_cast<const AirlineEntry*>(elem);
    return strncmp(code, e->code, 3);
}

bool looksLikeFlightNumber(const char* callsign) {
    if (!callsign) return false;

    for (int i = 0; i < 3; i++) {
        if (callsign[i] < 'A' || callsign[i] > 'Z') return false;
    }

    return callsign[3] >= '0' && callsign[3] <= '9';
}

AirlineInfo airlineResolve(const char* callsign) {
    static constexpr AirlineInfo none{nullptr, nullptr};

    if (!looksLikeFlightNumber(callsign)) return none;

    char code[4];
    memcpy(code, callsign, 3);
    code[3] = '\0';

    const AirlineEntry* hit = static_cast<const AirlineEntry*>(
        bsearch(code, AIRLINES, AIRLINES_COUNT, sizeof(AIRLINES[0]), compareCode));

    if (!hit) return none;

    return AirlineInfo{hit->name, hit->iata[0] ? hit->iata : nullptr};
}
