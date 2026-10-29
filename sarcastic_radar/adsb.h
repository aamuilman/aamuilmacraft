#pragma once

#include <stdint.h>

#include "config.h"

struct Target {
    char  callsign[9];
    char  type[5];

    char  airline[AIRLINE_MAX + 1];

    char  flightNo[9];
    float dst_nm;
    float dir_deg;
    float track_deg;
    int   alt_ft;
    int   gs_kt;
    bool  emergency;
};

struct FetchResult {
    bool     ok;
    int      count;
    uint32_t total;
    int      httpCode;
    char     error[40];
};

FetchResult adsbFetch(Target* out, int maxTargets,
                      double lat, double lon, float radiusNm);
