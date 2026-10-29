#pragma once

struct AirlineInfo {
    const char* name;
    const char* iata;
};

AirlineInfo airlineResolve(const char* callsign);

bool looksLikeFlightNumber(const char* callsign);
