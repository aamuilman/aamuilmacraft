#include "gesture.h"

namespace {
inline int heldCount(const bool dn[3]) {
    return (dn[0] ? 1 : 0) + (dn[1] ? 1 : 0) + (dn[2] ? 1 : 0);
}

inline int soleHeld(const bool dn[3]) {
    for (int i = 0; i < 3; ++i) {
        if (dn[i]) return i;
    }
    return -1;
}

SysEvent chordOf(const bool dn[3]) {
    const bool a = dn[(int)BtnId::A];
    const bool b = dn[(int)BtnId::B];

    if (a && b) return SysEvent::WifiWindow;
    return SysEvent::None;
}

inline void put(InputEvent* out, int maxOut, int& n, const InputEvent& e) {
    if (n < maxOut) out[n++] = e;
}
}

void gest::reset(State& st) {
    st = State{};
}

int gest::step(State& st, const bool dn[3], uint32_t now,
               const Timings& t, InputEvent* out, int maxOut) {
    int n = 0;

    for (int i = 0; i < 3; ++i) {
        if (dn[i] && !st.down[i]) {
            st.down[i]      = true;
            st.downMs[i]    = now;
            st.longFired[i] = false;
            st.consumed[i]  = false;
            continue;
        }
        if (!dn[i] && st.down[i]) {
            const uint32_t heldMs = now - st.downMs[i];
            st.down[i] = false;

            if (st.consumed[i] || st.longFired[i]) continue;

            const bool     isPower = (i == (int)BtnId::B);
            const uint32_t need    = isPower ? t.powerOffMs : t.longMs;

            if (heldMs >= need) {
                st.longFired[i] = true;

                InputEvent e;
                if (isPower) {
                    e.kind = InputEvent::Kind::Sys;
                    e.sys  = SysEvent::PowerOff;
                } else {
                    e.kind    = InputEvent::Kind::Button;
                    e.btn     = (BtnId)i;
                    e.gesture = Gesture::Long;
                }
                put(out, maxOut, n, e);
            } else if (heldMs < t.longMs) {
                InputEvent e;
                e.kind    = InputEvent::Kind::Button;
                e.btn     = (BtnId)i;
                e.gesture = Gesture::Short;
                put(out, maxOut, n, e);
            }
        }
    }

    const int cnt = heldCount(dn);
    if (cnt != st.prevN) {
        st.pairMs = now;
        st.prevN  = cnt;
        if (cnt == 0) st.chordFired = false;
    }

    if (cnt >= 2 && !st.chordFired && (now - st.pairMs) >= t.chordMs) {
        for (int i = 0; i < 3; ++i) {
            if (!dn[i]) continue;
            st.consumed[i]  = true;
            st.longFired[i] = true;
        }
        st.chordFired = true;

        const SysEvent se = chordOf(dn);
        if (se != SysEvent::None) {
            InputEvent e;
            e.kind = InputEvent::Kind::Sys;
            e.sys  = se;
            put(out, maxOut, n, e);
        }
    }

    if (cnt == 1) {
        const int i = soleHeld(dn);
        if (i >= 0 && !st.consumed[i] && !st.longFired[i]) {
            const bool     isPower = (i == (int)BtnId::B);
            const uint32_t need    = isPower ? t.powerOffMs : t.longMs;
            if ((now - st.downMs[i]) >= need) {
                st.longFired[i] = true;

                InputEvent e;
                if (isPower) {
                    e.kind = InputEvent::Kind::Sys;
                    e.sys  = SysEvent::PowerOff;
                } else {
                    e.kind    = InputEvent::Kind::Button;
                    e.btn     = (BtnId)i;
                    e.gesture = Gesture::Long;
                }
                put(out, maxOut, n, e);
            }
        }
    }

    return n;
}

int gest::stepPwr(PwrState& st, bool down, uint32_t now,
                  const Timings& t, InputEvent* out, int maxOut) {
    if (down && !st.down) {
        st.down   = true;
        st.downMs = now;
        st.fired  = false;
        return 0;
    }
    if (!down) {
        st.down   = false;
        st.fired  = false;
        return 0;
    }
    if (st.fired || (now - st.downMs) < t.pwrHoldMs) return 0;

    st.fired = true;

    int n = 0;
    InputEvent e;
    e.kind = InputEvent::Kind::Sys;
    e.sys  = SysEvent::PowerOff;
    put(out, maxOut, n, e);
    return n;
}
