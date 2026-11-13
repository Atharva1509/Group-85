// parse.cpp - splitting input lines and reading small values.
#include "erp.h"

#include <cctype>

namespace {

bool isGap(char ch) { return ch == ' ' || ch == '\t' || ch == '\r'; }

}  // namespace

void splitWords(const std::string& line, Words* out) {
    out->n = 0;
    int len = (int)line.size();
    int pos = 0;
    while (pos < len) {
        while (pos < len && isGap(line[pos])) ++pos;
        if (pos >= len) break;
        int end = pos;
        while (end < len && !isGap(line[end])) ++end;
        if (out->n < MAX_WORDS) out->w[out->n] = line.substr(pos, end - pos);
        ++out->n;                       // counted even when there is no room to keep it
        pos = end;
    }
}

// Digits with an optional leading '-'. Absurdly long numbers saturate near
// 10^9 instead of overflowing, so the caller's range check rejects them.
bool parseInt(const std::string& text, int* result) {
    int len = (int)text.size();
    int pos = 0;
    bool negative = false;
    if (len > 0 && text[0] == '-') {
        negative = true;
        pos = 1;
    }
    if (pos >= len) return false;

    int value = 0;
    for (; pos < len; ++pos) {
        if (!std::isdigit((unsigned char)text[pos])) return false;
        if (value < 100000000) {
            value = value * 10 + (text[pos] - '0');
        } else {
            value = 999999999;
        }
    }
    *result = negative ? -value : value;
    return true;
}

bool parseTermNo(const std::string& text, int* term) {
    int value = 0;
    if (!parseInt(text, &value)) return false;
    if (value < FIRST_TERM || value > LAST_TERM) return false;
    *term = value;
    return true;
}

bool idTooLong(const std::string& text, int limit) {
    return (int)text.size() > limit;
}

bool parseCourseType(const std::string& text, CourseType* type) {
    if (text == "REGULAR") { *type = REGULAR; return true; }
    if (text == "SU")      { *type = SU;      return true; }
    if (text == "THESIS")  { *type = THESIS;  return true; }
    return false;
}

bool parseCategory(const std::string& text, bool* isCore) {
    if (text == "CORE")     { *isCore = true;  return true; }
    if (text == "ELECTIVE") { *isCore = false; return true; }
    return false;
}

const char* courseTypeName(CourseType type) {
    switch (type) {
        case SU:     return "SU";
        case THESIS: return "THESIS";
        case REGULAR: break;
    }
    return "REGULAR";
}

const char* categoryName(bool isCore) {
    return isCore ? "CORE" : "ELECTIVE";
}

const char* errorName(ErrorCode code) {
    static const char* const names[] = {
        "OK",
        "E_PARSE", "E_FULL", "E_DUPLICATE", "E_UNKNOWN_STUDENT", "E_UNKNOWN_COURSE",
        "E_BAD_COURSE", "E_BAD_GRADE", "E_GRADE_TYPE", "E_NOT_REGISTERED",
        "E_ALREADY_GRADED", "E_THESIS_STATE", "E_ALREADY_PASSED", "E_ALREADY_REGISTERED",
        "E_PREREQ", "E_CREDIT_LIMIT", "E_THESIS_NOT_ALLOWED", "E_NO_GPA",
        "E_IO"
    };
    const int count = (int)(sizeof(names) / sizeof(names[0]));
    int index = (int)code;
    if (index < 0 || index >= count) return "E_UNKNOWN";
    return names[index];
}
