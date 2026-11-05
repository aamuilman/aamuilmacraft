#ifndef SLANG_SETUP_PORTAL_H
#define SLANG_SETUP_PORTAL_H

#include <stdint.h>

#include <M5GFX.h>

namespace portal {

enum Result : uint8_t {
    TIMEOUT = 0,
    SAVED,
};

Result run(M5Canvas& canvas, void (*presentFn)(void), uint32_t timeoutMs);

}
#endif
