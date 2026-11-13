// db.cpp - the catalogue, the student table and every business rule.
// Rule of thumb used throughout: validate first, mutate last, so a command
// that fails leaves the stored data exactly as it was.
#include "erp.h"

AcadDB::AcadDB() : prog_(), nCourses_(0), nStudents_(0) {
    for (int i = 0; i < MAX_COURSES; ++i) courses_[i] = nullptr;
    for (int i = 0; i < MAX_STUDENTS; ++i) students_[i] = nullptr;
}

// Releases every object allocated with new in addCourse / addStudent.
AcadDB::~AcadDB() {
    for (int i = 0; i < nCourses_; ++i) delete courses_[i];
    for (int i = 0; i < nStudents_; ++i) delete students_[i];
}

int AcadDB::findStudent(const std::string& roll) const {
    for (int i = 0; i < nStudents_; ++i) {
        if (students_[i]->roll() == roll) return i;
    }
    return -1;
}

int AcadDB::findCourse(const std::string& code) const {
    for (int i = 0; i < nCourses_; ++i) {
        if (courses_[i]->code() == code) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Creating records
// ---------------------------------------------------------------------------
ErrorCode AcadDB::addCourse(const std::string& code, CourseType type, int credits,
                            bool core, const std::string& prereqCode) {
    if (nCourses_ >= MAX_COURSES) return E_FULL;
    if (findCourse(code) >= 0) return E_DUPLICATE;

    int lowest = (type == SU) ? 0 : 1;              // 0 credits only for SU
    if (credits < lowest || credits > MAX_CREDITS) return E_BAD_COURSE;

    int prereq = -1;
    if (prereqCode != "-") {
        if (prereqCode == code) return E_BAD_COURSE;   // a course cannot require itself
        prereq = findCourse(prereqCode);
        if (prereq < 0) return E_BAD_COURSE;           // must already exist
    }

    courses_[nCourses_] = makeCourse(type, code, credits, core, prereq);
    ++nCourses_;
    return OK;
}

ErrorCode AcadDB::addStudent(const std::string& roll, const std::string& name) {
    if (nStudents_ >= MAX_STUDENTS) return E_FULL;
    if (findStudent(roll) >= 0) return E_DUPLICATE;
    students_[nStudents_] = new Student(roll, name);
    ++nStudents_;
    return OK;
}

// ---------------------------------------------------------------------------
// Small questions about one student
// ---------------------------------------------------------------------------
// The counting attempt: the newest registration of course c that has a result.
int AcadDB::countingAttempt(int s, int c) const {
    const Student& st = *students_[s];
    for (int i = st.regCount() - 1; i >= 0; --i) {
        if (st.reg(i).course() == c && courses_[c]->hasResult(st.reg(i))) return i;
    }
    return -1;
}

bool AcadDB::passed(int s, int c) const {
    const Student& st = *students_[s];
    int attempt = countingAttempt(s, c);
    return attempt >= 0 && courses_[c]->isPass(st.reg(attempt));
}

bool AcadDB::hasOpenRegistration(int s, int c) const {
    const Student& st = *students_[s];
    for (int i = 0; i < st.regCount(); ++i) {
        if (st.reg(i).course() == c && courses_[c]->isOpen(st.reg(i))) return true;
    }
    return false;
}

// Credits of every registration in the term, whatever its grade.
int AcadDB::termLoad(int s, int term) const {
    const Student& st = *students_[s];
    int load = 0;
    for (int i = 0; i < st.regCount(); ++i) {
        if (st.reg(i).term() == term) load += courses_[st.reg(i).course()]->credits();
    }
    return load;
}

// Each catalogue course contributes once, through its counting attempt.
int AcadDB::earnedCredits(int s) const {
    int total = 0;
    for (int c = 0; c < nCourses_; ++c) {
        if (passed(s, c)) total += courses_[c]->credits();
    }
    return total;
}

int AcadDB::electivePassedCredits(int s) const {
    int total = 0;
    for (int c = 0; c < nCourses_; ++c) {
        if (!courses_[c]->isCore() && passed(s, c)) total += courses_[c]->credits();
    }
    return total;
}

bool AcadDB::allCorePassed(int s) const {
    for (int c = 0; c < nCourses_; ++c) {
        if (courses_[c]->isCore() && !passed(s, c)) return false;
    }
    return true;
}

// R4: some thesis course is FINAL with a letter of C or better.
bool AcadDB::thesisBarMet(int s) const {
    const Student& st = *students_[s];
    for (int c = 0; c < nCourses_; ++c) {
        if (courses_[c]->type() != THESIS) continue;
        int attempt = countingAttempt(s, c);
        if (attempt < 0) continue;
        const Registration& r = st.reg(attempt);
        if (r.thesisState() == FINAL && gradePoints(r.grade()) >= THESIS_BAR_POINTS) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Eligibility and registration
// ---------------------------------------------------------------------------
int AcadDB::checkEligibility(int s, int c, int term) const {
    if (s < 0 || c < 0) return 1;                                        // E1
    if (passed(s, c)) return 2;                                          // E2
    if (hasOpenRegistration(s, c)) return 3;                             // E3
    int pre = courses_[c]->prereq();
    if (pre >= 0 && !passed(s, pre)) return 4;                           // E4
    if (termLoad(s, term) + courses_[c]->credits() > prog_.maxLoad) return 5;   // E5
    if (courses_[c]->type() == THESIS && earnedCredits(s) < prog_.thesisCredits) return 6;  // E6
    return 0;
}

ErrorCode AcadDB::registerCourse(int s, int c, int term) {
    if (s < 0) return E_UNKNOWN_STUDENT;
    if (c < 0) return E_UNKNOWN_COURSE;
    switch (checkEligibility(s, c, term)) {
        case 0: break;
        case 2: return E_ALREADY_PASSED;
        case 3: return E_ALREADY_REGISTERED;
        case 4: return E_PREREQ;
        case 5: return E_CREDIT_LIMIT;
        case 6: return E_THESIS_NOT_ALLOWED;
        default: return E_PARSE;
    }
    return students_[s]->addRegistration(c, term, courses_[c]->initialState());
}

// ---------------------------------------------------------------------------
// Grading
// ---------------------------------------------------------------------------
ErrorCode AcadDB::submitGrade(int s, int c, int term, const std::string& symbol) {
    if (s < 0) return E_UNKNOWN_STUDENT;
    if (c < 0) return E_UNKNOWN_COURSE;
    if (!isKnownSymbol(symbol)) return E_BAD_GRADE;
    if (!courses_[c]->acceptsSymbol(symbol)) return E_GRADE_TYPE;

    Student& st = *students_[s];
    int r = st.latestFor(c, term);
    if (r < 0) return E_NOT_REGISTERED;
    return courses_[c]->record(st.reg(r), symbol);
}

ErrorCode AcadDB::convertThesis(int s, int c, const std::string& letter) {
    if (s < 0) return E_UNKNOWN_STUDENT;
    if (c < 0) return E_UNKNOWN_COURSE;
    if (!isKnownSymbol(letter)) return E_BAD_GRADE;       // not a grade at all
    if (courses_[c]->type() != THESIS) return E_GRADE_TYPE;
    if (gradePoints(letter) < 0) return E_GRADE_TYPE;     // S / X: a valid symbol, but not a letter

    Student& st = *students_[s];
    int r = st.latestFor(c, 0);                           // CONVERT names no term: newest attempt
    if (r < 0) return E_NOT_REGISTERED;
    return courses_[c]->convert(st.reg(r), letter);
}

// ---------------------------------------------------------------------------
// GPA and graduation
// ---------------------------------------------------------------------------
ErrorCode AcadDB::gpaHundredths(int s, int* result) const {
    const Student& st = *students_[s];
    GpaAccum acc;
    for (int c = 0; c < nCourses_; ++c) {
        int attempt = countingAttempt(s, c);
        if (attempt < 0) continue;
        const Registration& r = st.reg(attempt);
        if (courses_[c]->inGpa(r)) acc.add(courses_[c]->credits(), gradePoints(r.grade()));
    }
    return acc.hundredths(result);
}

unsigned AcadDB::graduationFailures(int s) const {
    unsigned failed = 0;
    if (earnedCredits(s) < prog_.totalCredits) failed |= RULE_R1;
    if (!allCorePassed(s)) failed |= RULE_R2;
    if (electivePassedCredits(s) < prog_.electiveCredits) failed |= RULE_R3;
    if (!thesisBarMet(s)) failed |= RULE_R4;

    int h = 0;
    ErrorCode ec = gpaHundredths(s, &h);
    if (ec != OK || h < prog_.minGpaHundredths) failed |= RULE_R5;   // no GPA data also fails R5
    return failed;
}

// ---------------------------------------------------------------------------
// LOAD support
// ---------------------------------------------------------------------------
ErrorCode AcadDB::restoreRegistration(const std::string& roll, const std::string& code,
                                      int term, const std::string& grade, ThesisState state) {
    int s = findStudent(roll);
    if (s < 0) return E_UNKNOWN_STUDENT;
    int c = findCourse(code);
    if (c < 0) return E_UNKNOWN_COURSE;
    if (!courses_[c]->validRestore(grade, state)) return E_PARSE;

    Student& st = *students_[s];
    ErrorCode ec = st.addRegistration(c, term, state);
    if (ec != OK) return ec;
    st.reg(st.regCount() - 1).setGrade(grade);
    return OK;
}

// Exchanges everything; the object that ends up holding the old data frees it
// in its own destructor, so nothing is leaked and nothing is freed twice.
void AcadDB::swapContents(AcadDB& other) {
    Program tmpProg = prog_;
    prog_ = other.prog_;
    other.prog_ = tmpProg;

    for (int i = 0; i < MAX_COURSES; ++i) {
        Course* tmp = courses_[i];
        courses_[i] = other.courses_[i];
        other.courses_[i] = tmp;
    }
    for (int i = 0; i < MAX_STUDENTS; ++i) {
        Student* tmp = students_[i];
        students_[i] = other.students_[i];
        other.students_[i] = tmp;
    }
    int n = nCourses_;
    nCourses_ = other.nCourses_;
    other.nCourses_ = n;
    n = nStudents_;
    nStudents_ = other.nStudents_;
    other.nStudents_ = n;
}
