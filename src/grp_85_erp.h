// erp.h - shared declarations for AcadERP-Lite (academic records and graduation audit).
//
// Layout of the code base
//   grading.cpp   grade scale, exact GPA arithmetic, average-grade bands
//   parse.cpp     word splitting, number parsing, keyword tables
//   student.cpp   Registration and Student (owns its registration array)
//   course.cpp    Course hierarchy: Regular / Su / Thesis (virtual dispatch)
//   db.cpp        AcadDB: the catalogue + student table and all business rules
//   commands.cpp  one handler per input command, and the Session dispatcher
//   persist.cpp   SAVE / LOAD (bonus), written with <cstdio> only
//   main.cpp      line reader
#ifndef ACADERP_ERP_H
#define ACADERP_ERP_H

#include <iostream>
#include <string>

// Every function that can fail returns an ErrorCode. On GCC/Clang the compiler
// itself warns when a caller forgets to look at the result.
#if defined(__GNUC__)
#define MUST_CHECK __attribute__((warn_unused_result))
#else
#define MUST_CHECK
#endif

// ---------------------------------------------------------------------------
// Limits from the brief
// ---------------------------------------------------------------------------
const int MAX_STUDENTS = 100;
const int MAX_COURSES  = 50;
const int MAX_REGS     = 40;     // registrations per student
const int MAX_CODE_LEN = 10;     // course codes and roll numbers
const int MAX_NAME_LEN = 30;
const int FIRST_TERM   = 1;
const int LAST_TERM    = 12;
const int MAX_CREDITS  = 24;
const int MAX_WORDS    = 8;      // words kept per input line (more are counted only)

// Grade points that matter for decisions
const int PASS_POINTS       = 4; // D
const int THESIS_BAR_POINTS = 6; // C

// ---------------------------------------------------------------------------
// Error codes
// ---------------------------------------------------------------------------
enum ErrorCode {
    OK,
    E_PARSE, E_FULL, E_DUPLICATE, E_UNKNOWN_STUDENT, E_UNKNOWN_COURSE,
    E_BAD_COURSE, E_BAD_GRADE, E_GRADE_TYPE, E_NOT_REGISTERED,
    E_ALREADY_GRADED, E_THESIS_STATE, E_ALREADY_PASSED, E_ALREADY_REGISTERED,
    E_PREREQ, E_CREDIT_LIMIT, E_THESIS_NOT_ALLOWED, E_NO_GPA,
    E_IO                                   // bonus only: SAVE / LOAD file trouble
};

enum CourseType  { REGULAR, SU, THESIS };
enum ThesisState { NOT_THESIS, REGISTERED, IN_PROGRESS, FINAL, FAILED };

const char* errorName(ErrorCode code);

// ---------------------------------------------------------------------------
// grading.cpp - scale, GPA, bands
// ---------------------------------------------------------------------------
int  gradePoints(const std::string& symbol);      // -1 when not a letter grade
bool isKnownSymbol(const std::string& symbol);    // letter grades plus S and X

// Credit-weighted mean kept as two integers; nothing here touches floating point.
class GpaAccum {
public:
    GpaAccum() : num_(0), den_(0) {}
    void add(int credits, int points) {
        num_ += (long)credits * points;
        den_ += credits;
    }
    bool empty() const { return den_ == 0; }
    // h = floor((200n + d) / 2d): the GPA in hundredths, rounded half-up.
    MUST_CHECK ErrorCode hundredths(int* result) const;
private:
    long num_;
    long den_;
};

const char* bandLetter(int hundredths);           // average grade for a rounded GPA
std::string formatHundredths(int hundredths);     // 813 -> "8.13"
bool parseHundredths(const std::string& text, int* result);  // "5.00" -> 500

// ---------------------------------------------------------------------------
// parse.cpp - text helpers
// ---------------------------------------------------------------------------
struct Words {
    std::string w[MAX_WORDS];
    int n;                                        // real word count, may exceed MAX_WORDS
    Words() : n(0) {}
};

void splitWords(const std::string& line, Words* out);
bool parseInt(const std::string& text, int* result);       // optional '-', digits only
bool parseTermNo(const std::string& text, int* term);      // 1..12 only
bool idTooLong(const std::string& text, int limit);
bool parseCourseType(const std::string& text, CourseType* type);
bool parseCategory(const std::string& text, bool* isCore);
const char* courseTypeName(CourseType type);
const char* categoryName(bool isCore);

// ---------------------------------------------------------------------------
// student.cpp - one registration, one student
// ---------------------------------------------------------------------------
class Registration {
public:
    Registration() : course_(-1), term_(0), grade_(), state_(NOT_THESIS) {}
    void open(int course, int term, ThesisState initial);

    int course() const { return course_; }
    int term() const { return term_; }
    const std::string& grade() const { return grade_; }
    ThesisState thesisState() const { return state_; }
    bool hasGrade() const { return !grade_.empty(); }

    void setGrade(const std::string& symbol) { grade_ = symbol; }
    void setThesisState(ThesisState state) { state_ = state; }
private:
    int course_;              // index into the catalogue
    int term_;
    std::string grade_;       // "" while ungraded; "S"/"X" for a progress grade
    ThesisState state_;
};

class Student {
public:
    Student(const std::string& roll, const std::string& name);
    ~Student();

    const std::string& roll() const { return roll_; }
    const std::string& name() const { return name_; }
    int regCount() const { return numRegs_; }
    const Registration& reg(int i) const { return regs_[i]; }
    Registration& reg(int i) { return regs_[i]; }

    MUST_CHECK ErrorCode addRegistration(int course, int term, ThesisState initial);
    // Newest registration of `course` (in `term`, or in any term when term == 0); -1 if none.
    int latestFor(int course, int term) const;
private:
    Student(const Student&) = delete;
    Student& operator=(const Student&) = delete;

    std::string roll_;
    std::string name_;
    Registration* regs_;      // new Registration[MAX_REGS] in the constructor, delete[] in the destructor
    int numRegs_;
};

// ---------------------------------------------------------------------------
// course.cpp - the three kinds of course
// ---------------------------------------------------------------------------
class Course {
public:
    Course(const std::string& code, int credits, bool core, int prereq)
        : code_(code), credits_(credits), core_(core), prereq_(prereq) {}
    virtual ~Course() {}

    const std::string& code() const { return code_; }
    int credits() const { return credits_; }
    bool isCore() const { return core_; }
    int prereq() const { return prereq_; }      // catalogue index or -1

    virtual CourseType type() const = 0;
    virtual ThesisState initialState() const;                     // NOT_THESIS unless thesis
    virtual bool acceptsSymbol(const std::string& symbol) const = 0;
    virtual ErrorCode record(Registration& r, const std::string& symbol) const = 0;
    virtual ErrorCode convert(Registration& r, const std::string& letter) const;
    virtual bool hasResult(const Registration& r) const = 0;     // can this attempt be the counting attempt?
    virtual bool isOpen(const Registration& r) const = 0;        // blocks a new registration
    virtual bool isPass(const Registration& r) const = 0;        // judged on the counting attempt
    virtual bool inGpa(const Registration& r) const = 0;
    virtual bool validRestore(const std::string& grade, ThesisState state) const = 0;
private:
    std::string code_;
    int credits_;
    bool core_;
    int prereq_;
};

Course* makeCourse(CourseType type, const std::string& code, int credits, bool core, int prereq);

// ---------------------------------------------------------------------------
// db.cpp - catalogue, students, rules
// ---------------------------------------------------------------------------
struct Program {
    int totalCredits;
    int electiveCredits;
    int minGpaHundredths;
    int maxLoad;
    int thesisCredits;
    Program()
        : totalCredits(160), electiveCredits(24), minGpaHundredths(500),
          maxLoad(24), thesisCredits(100) {}
};

// Bit masks returned by AcadDB::graduationFailures(): bit k is rule R(k+1).
const unsigned RULE_R1 = 1u << 0;
const unsigned RULE_R2 = 1u << 1;
const unsigned RULE_R3 = 1u << 2;
const unsigned RULE_R4 = 1u << 3;
const unsigned RULE_R5 = 1u << 4;

class AcadDB {
public:
    AcadDB();
    ~AcadDB();

    const Program& program() const { return prog_; }
    void setProgram(const Program& p) { prog_ = p; }

    int courseCount() const { return nCourses_; }
    int studentCount() const { return nStudents_; }
    const Course& courseAt(int i) const { return *courses_[i]; }
    const Student& studentAt(int i) const { return *students_[i]; }

    int findStudent(const std::string& roll) const;   // -1 when unknown
    int findCourse(const std::string& code) const;    // -1 when unknown

    MUST_CHECK ErrorCode addCourse(const std::string& code, CourseType type, int credits,
                                   bool core, const std::string& prereqCode);
    MUST_CHECK ErrorCode addStudent(const std::string& roll, const std::string& name);
    MUST_CHECK ErrorCode registerCourse(int s, int c, int term);
    MUST_CHECK ErrorCode submitGrade(int s, int c, int term, const std::string& symbol);
    MUST_CHECK ErrorCode convertThesis(int s, int c, const std::string& letter);
    MUST_CHECK ErrorCode gpaHundredths(int s, int* result) const;

    // 0 when the student may register, otherwise k for check Ek (E1 covers unknown ids).
    int checkEligibility(int s, int c, int term) const;
    int earnedCredits(int s) const;
    unsigned graduationFailures(int s) const;         // 0 means the student graduates

    // Used by LOAD: put a registration back exactly as it was saved.
    MUST_CHECK ErrorCode restoreRegistration(const std::string& roll, const std::string& code,
                                             int term, const std::string& grade,
                                             ThesisState state);
    void swapContents(AcadDB& other);

private:
    AcadDB(const AcadDB&) = delete;
    AcadDB& operator=(const AcadDB&) = delete;

    int countingAttempt(int s, int c) const;          // newest attempt with a result, or -1
    bool passed(int s, int c) const;
    bool hasOpenRegistration(int s, int c) const;
    int termLoad(int s, int term) const;
    int electivePassedCredits(int s) const;
    bool allCorePassed(int s) const;
    bool thesisBarMet(int s) const;

    Program prog_;
    Course* courses_[MAX_COURSES];        // each created with new in addCourse, deleted in ~AcadDB
    int nCourses_;
    Student* students_[MAX_STUDENTS];     // each created with new in addStudent, deleted in ~AcadDB
    int nStudents_;
};

// ---------------------------------------------------------------------------
// commands.cpp - the interpreter
// ---------------------------------------------------------------------------
class Session {
public:
    Session() : seenCommand_(false) {}
    // Runs one input line. Queries print their single answer line themselves.
    MUST_CHECK ErrorCode execute(const std::string& line);
private:
    AcadDB db_;
    bool seenCommand_;
};

// ---------------------------------------------------------------------------
// persist.cpp - bonus SAVE / LOAD
// ---------------------------------------------------------------------------
MUST_CHECK ErrorCode saveDatabase(const AcadDB& db, const std::string& path);
MUST_CHECK ErrorCode loadDatabase(AcadDB* db, const std::string& path);

#endif  // ACADERP_ERP_H
