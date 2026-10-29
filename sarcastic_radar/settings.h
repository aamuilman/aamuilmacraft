#pragma once

#include <stddef.h>
#include <stdint.h>

struct Settings {
    char   ssid[33];
    char   pass[65];
    double lat;
    double lon;
    float  radiusKm;
    int    utcOffsetMin;
    char   metar[5];

    bool valid() const;
};

namespace settings {
const char* validate(const Settings& s);

bool load(Settings& out);

bool store(const Settings& s);

void clear();

bool parse(const char* ssid, const char* pass,
           const char* lat, const char* lon, const char* radius,
           const char* tzHours, const char* metar,
           Settings& out, char* err, size_t errLen);
}
