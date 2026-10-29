#pragma once

#include <stdint.h>

static constexpr int METAR_RAW_MAX = 192;

struct Metar {
    bool ok;
    int  httpCode;

    char raw[METAR_RAW_MAX];
    char error[48];
};

void metarFetch(Metar& m, const char* station);
