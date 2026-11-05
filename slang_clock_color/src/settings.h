#ifndef SLANG_SETTINGS_H
#define SLANG_SETTINGS_H

#include <stdint.h>

#include "slang_engine.h"

namespace settings {

struct Data {
    uint32_t        magic;
    uint32_t        version;
    slang::Schedule sched;
    int16_t         tzOffsetMin;
    char            ssid[33];
    char            pass[65];
    uint8_t         configured;
};

Data& data();

void defaults();
bool load();
bool save();

}
#endif
