// grading.cpp - grade scale, exact GPA arithmetic, average-grade bands.
#include "erp.h"

#include <cctype>
#include <cstdio>

namespace {

struct ScaleRow {
    const char* symbol;
    int points;
};

const ScaleRow kScale[] = {
    {"A", 10}, {"A-", 9}, {"B", 8}, {"B-", 7},
    {"C", 6},  {"C-", 5}, {"D", 4}, {"F", 0}
};
const int kScaleRows = (int)(sizeof(kScale) / sizeof(kScale[0]));

struct Band {
    int lowEdge;            // inclusive, in hundredths
    const char* letter;
};

// Scanned top-down, so each band owns everything from its low edge upwards.
const Band kBands[] = {
    {950, "A"}, {850, "A-"}, {750, "B"}, {650, "B-"},
    {550, "C"}, {450, "C-"}, {200, "D"}, {0, "F"}
};
const int kBandRows = (int)(sizeof(kBands) / sizeof(kBands[0]));

}  // namespace

int gradePoints(const std::string& symbol) {
    for (int i = 0; i < kScaleRows; ++i) {
        if (symbol == kScale[i].symbol) return kScale[i].points;
    }
    return -1;
}

bool isKnownSymbol(const std::string& symbol) {
    return gradePoints(symbol) >= 0 || symbol == "S" || symbol == "X";
}

ErrorCode GpaAccum::hundredths(int* result) const {
    if (den_ == 0) return E_NO_GPA;
    *result = (int)((200 * num_ + den_) / (2 * den_));   // integer division floors
    return OK;
}

const char* bandLetter(int hundredths) {
    for (int i = 0; i < kBandRows; ++i) {
        if (hundredths >= kBands[i].lowEdge) return kBands[i].letter;
    }
    return "F";
}

std::string formatHundredths(int hundredths) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%d.%02d", hundredths / 100, hundredths % 100);
    return std::string(buf);
}

// Accepts "5", "5.0" and "5.00" (at most two decimals), value 0.00 to 10.00.
bool parseHundredths(const std::string& text, int* result) {
    int len = (int)text.size();
    int i = 0;
    int whole = 0;
    int wholeDigits = 0;
    while (i < len && std::isdigit((unsigned char)text[i])) {
        whole = whole * 10 + (text[i] - '0');
        ++wholeDigits;
        ++i;
        if (wholeDigits > 3) return false;
    }
    if (wholeDigits == 0) return false;

    int frac = 0;
    if (i < len && text[i] == '.') {
        ++i;
        int fracDigits = 0;
        while (i < len && std::isdigit((unsigned char)text[i])) {
            if (fracDigits == 2) return false;
            frac = frac * 10 + (text[i] - '0');
            ++fracDigits;
            ++i;
        }
        if (fracDigits == 0) return false;
        if (fracDigits == 1) frac *= 10;
    }
    if (i != len) return false;

    int value = whole * 100 + frac;
    if (value > 1000) return false;
    *result = value;
    return true;
}
