#pragma once

#include "gesture.h"
#include "settings.h"

namespace radar {
void begin(const Settings& cfg);

bool due();

void requestRefresh();

void tick();

void onButton(BtnId btn, Gesture g);
}
