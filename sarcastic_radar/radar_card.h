#pragma once

#include <stdint.h>

#include "adsb.h"

static constexpr int CARD_LINE_MAX = 40;

static constexpr int CARD_WRAP_LINES = 8;

enum class CardKind : uint8_t {
    Emergency = 0,
    Business  = 1,
    Window    = 2,
};

struct CardRecord {
    bool has;
    char callsign[9];
    char value[12];
};

struct CardRecords {
    CardRecord far;
    CardRecord high;
    CardRecord fast;
    CardRecord near;
};

struct CardInfo {
    bool     has;
    CardKind kind;
    char     bort[32];
    char     callsign[9];
    char     alt[20];
    char     spd[20];
    int      phrase;
};

bool cardIsBusinessJet(const char* type);

const char* cardModelName(const char* type);

int cardPickIndex(const Target* targets, int count);

CardInfo cardBuild(const Target& t);

CardRecords cardRecords(const Target* targets, int count);

int         cardPhraseCount(CardKind k);
const char* cardPhrase(CardKind k, int idx);

int cardPhrasePick(CardKind k, const char* callsign);

int cardPhraseText(CardKind k, int idx, const char* callsign,
                   char* out, int cap);

int cardFtToM(int ft);
int cardKtToKmh(int kt);

void cardKmText(char* out, int cap, float nm);

int cardWrap(const char* text, int px, int maxW,
             char out[][CARD_LINE_MAX], int maxLines);
