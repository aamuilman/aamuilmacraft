#pragma once

#include <stdint.h>

enum class BtnId : uint8_t { A = 0, B, C, COUNT };

enum class Gesture : uint8_t {
    Short,
    Long,
};

enum class SysEvent : uint8_t {
    None = 0,
    WifiWindow,
    PowerOff,
};

struct InputEvent {
    enum class Kind : uint8_t { None = 0, Button, Sys };

    Kind     kind    = Kind::None;
    BtnId    btn     = BtnId::A;
    Gesture  gesture = Gesture::Short;
    SysEvent sys     = SysEvent::None;
};

namespace gest {
struct Timings {
    uint32_t longMs;
    uint32_t chordMs;
    uint32_t powerOffMs;
    uint32_t pwrHoldMs;
};

struct State {
    bool     down[3]      = { false, false, false };
    uint32_t downMs[3]    = { 0, 0, 0 };
    bool     longFired[3] = { false, false, false };
    bool     consumed[3]  = { false, false, false };

    int      prevN      = 0;
    uint32_t pairMs     = 0;
    bool     chordFired = false;
};

void reset(State& st);

int step(State& st, const bool dn[3], uint32_t now,
         const Timings& t, InputEvent* out, int maxOut);

struct PwrState {
    bool     down   = false;
    uint32_t downMs = 0;
    bool     fired  = false;
};

int stepPwr(PwrState& st, bool down, uint32_t now, const Timings& t,
            InputEvent* out, int maxOut);
}
