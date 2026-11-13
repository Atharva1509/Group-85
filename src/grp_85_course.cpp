// course.cpp - the three course kinds. Each one answers the same questions
// (what symbols do you accept, is this attempt passed, does it count in the
// GPA, does it block a new registration) in its own way, so the rest of the
// program never switches on the course type.
#include "erp.h"

ThesisState Course::initialState() const {
    return NOT_THESIS;
}

// Only a thesis can be converted; every other kind reports a type mismatch.
ErrorCode Course::convert(Registration&, const std::string&) const {
    return E_GRADE_TYPE;
}

namespace {

bool isProgressSymbol(const std::string& s) {
    return s == "S" || s == "X";
}

// ---------------------------------------------------------------------------
// REGULAR: letter grades, counted in the GPA
// ---------------------------------------------------------------------------
class RegularCourse : public Course {
public:
    RegularCourse(const std::string& code, int credits, bool core, int prereq)
        : Course(code, credits, core, prereq) {}

    CourseType type() const override { return REGULAR; }

    bool acceptsSymbol(const std::string& symbol) const override {
        return gradePoints(symbol) >= 0;
    }
    ErrorCode record(Registration& r, const std::string& symbol) const override {
        if (r.hasGrade()) return E_ALREADY_GRADED;
        r.setGrade(symbol);
        return OK;
    }
    bool hasResult(const Registration& r) const override { return r.hasGrade(); }
    bool isOpen(const Registration& r) const override { return !r.hasGrade(); }
    bool isPass(const Registration& r) const override {
        return r.hasGrade() && gradePoints(r.grade()) >= PASS_POINTS;
    }
    bool inGpa(const Registration& r) const override { return r.hasGrade(); }
    bool validRestore(const std::string& grade, ThesisState state) const override {
        return state == NOT_THESIS && (grade.empty() || gradePoints(grade) >= 0);
    }
};

// ---------------------------------------------------------------------------
// SU: satisfactory / unsatisfactory, never in the GPA
// ---------------------------------------------------------------------------
class SuCourse : public Course {
public:
    SuCourse(const std::string& code, int credits, bool core, int prereq)
        : Course(code, credits, core, prereq) {}

    CourseType type() const override { return SU; }

    bool acceptsSymbol(const std::string& symbol) const override {
        return isProgressSymbol(symbol);
    }
    ErrorCode record(Registration& r, const std::string& symbol) const override {
        if (r.hasGrade()) return E_ALREADY_GRADED;
        r.setGrade(symbol);
        return OK;
    }
    bool hasResult(const Registration& r) const override { return r.hasGrade(); }
    bool isOpen(const Registration& r) const override { return !r.hasGrade(); }
    bool isPass(const Registration& r) const override { return r.grade() == "S"; }
    bool inGpa(const Registration&) const override { return false; }
    bool validRestore(const std::string& grade, ThesisState state) const override {
        return state == NOT_THESIS && (grade.empty() || isProgressSymbol(grade));
    }
};

// ---------------------------------------------------------------------------
// THESIS: progress grade first, letter grade after CONVERT
// ---------------------------------------------------------------------------
class ThesisCourse : public Course {
public:
    ThesisCourse(const std::string& code, int credits, bool core, int prereq)
        : Course(code, credits, core, prereq) {}

    CourseType type() const override { return THESIS; }
    ThesisState initialState() const override { return REGISTERED; }

    bool acceptsSymbol(const std::string& symbol) const override {
        return isProgressSymbol(symbol);
    }
    // REGISTERED --S--> IN_PROGRESS,  REGISTERED --X--> FAILED
    ErrorCode record(Registration& r, const std::string& symbol) const override {
        if (r.thesisState() != REGISTERED) return E_ALREADY_GRADED;
        r.setGrade(symbol);
        r.setThesisState(symbol == "S" ? IN_PROGRESS : FAILED);
        return OK;
    }
    // IN_PROGRESS --letter--> FINAL
    ErrorCode convert(Registration& r, const std::string& letter) const override {
        if (gradePoints(letter) < 0) return E_GRADE_TYPE;
        if (r.thesisState() == FINAL) return E_ALREADY_GRADED;
        if (r.thesisState() != IN_PROGRESS) return E_THESIS_STATE;
        r.setGrade(letter);
        r.setThesisState(FINAL);
        return OK;
    }
    // A progress grade S is not a result yet: only FINAL (letter) or FAILED (X) are.
    bool hasResult(const Registration& r) const override {
        return r.thesisState() == FINAL || r.thesisState() == FAILED;
    }
    bool isOpen(const Registration& r) const override {
        return r.thesisState() == REGISTERED || r.thesisState() == IN_PROGRESS;
    }
    bool isPass(const Registration& r) const override {
        return r.thesisState() == FINAL && gradePoints(r.grade()) >= PASS_POINTS;
    }
    bool inGpa(const Registration& r) const override { return r.thesisState() == FINAL; }
    bool validRestore(const std::string& grade, ThesisState state) const override {
        switch (state) {
            case REGISTERED:  return grade.empty();
            case IN_PROGRESS: return grade == "S";
            case FAILED:      return grade == "X";
            case FINAL:       return gradePoints(grade) >= 0;
            case NOT_THESIS:  break;
        }
        return false;
    }
};

}  // namespace

Course* makeCourse(CourseType type, const std::string& code, int credits, bool core, int prereq) {
    switch (type) {
        case SU:     return new SuCourse(code, credits, core, prereq);
        case THESIS: return new ThesisCourse(code, credits, core, prereq);
        case REGULAR: break;
    }
    return new RegularCourse(code, credits, core, prereq);
}
