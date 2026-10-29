#include "utf8.h"

namespace utf8 {
uint32_t next(const char** p) {
    if (!p || !*p || !**p) return 0;

    const unsigned char* s = (const unsigned char*)*p;
    const unsigned char  b = s[0];

    if (b < 0x80) {
        *p += 1;
        return b;
    }

    int len;
    uint32_t cp;
    if ((b & 0xE0) == 0xC0) { len = 2; cp = b & 0x1Fu; }
    else if ((b & 0xF0) == 0xE0) { len = 3; cp = b & 0x0Fu; }
    else if ((b & 0xF8) == 0xF0) { len = 4; cp = b & 0x07u; }
    else { *p += 1; return REPLACEMENT; }

    for (int i = 1; i < len; i++) {
        const unsigned char c = s[i];

        if (c == 0) { *p += 1; return REPLACEMENT; }
        if ((c & 0xC0) != 0x80) { *p += 1; return REPLACEMENT; }
        cp = (cp << 6) | (c & 0x3Fu);
    }

    if (len == 2 && cp < 0x80) { *p += 1; return REPLACEMENT; }
    if (len == 3 && cp < 0x800) { *p += 1; return REPLACEMENT; }
    if (len == 4 && cp < 0x10000) { *p += 1; return REPLACEMENT; }
    if (cp >= 0xD800 && cp <= 0xDFFF) { *p += 1; return REPLACEMENT; }
    if (cp > 0x10FFFF) { *p += 1; return REPLACEMENT; }

    *p += len;
    return cp;
}

size_t count(const char* s) {
    if (!s) return 0;
    size_t n = 0;
    while (*s) {
        utf8::next(&s);
        n++;
    }
    return n;
}
}
