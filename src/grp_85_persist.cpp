// persist.cpp - bonus SAVE / LOAD.
// Plain <cstdio> (fopen / fgetc / fprintf) is enough for this bonus, which keeps
// the program inside the always-allowed header list.
//
// File format (text, one record per line):
//   ACADERP-LITE 1
//   P <total> <elective> <minGpaHundredths> <maxLoad> <thesisCredits>
//   C <code> <REGULAR|SU|THESIS> <credits> <CORE|ELECTIVE> <prereq|->
//   S <roll> <name>
//   R <roll> <course> <term> <grade|-> <thesisState 0..4>
#include "erp.h"

#include <cstdio>

namespace {

bool readLine(FILE* fp, std::string* out) {
    out->clear();
    int ch = std::fgetc(fp);
    if (ch == EOF) return false;
    while (ch != EOF && ch != '\n') {
        out->push_back((char)ch);
        ch = std::fgetc(fp);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------
bool writeProgram(FILE* fp, const Program& p) {
    return std::fprintf(fp, "P %d %d %d %d %d\n", p.totalCredits, p.electiveCredits,
                        p.minGpaHundredths, p.maxLoad, p.thesisCredits) > 0;
}

bool writeCourses(FILE* fp, const AcadDB& db) {
    for (int i = 0; i < db.courseCount(); ++i) {
        const Course& c = db.courseAt(i);
        std::string pre = "-";
        if (c.prereq() >= 0) pre = db.courseAt(c.prereq()).code();
        if (std::fprintf(fp, "C %s %s %d %s %s\n", c.code().c_str(), courseTypeName(c.type()),
                         c.credits(), categoryName(c.isCore()), pre.c_str()) < 0) {
            return false;
        }
    }
    return true;
}

bool writeStudents(FILE* fp, const AcadDB& db) {
    for (int i = 0; i < db.studentCount(); ++i) {
        const Student& st = db.studentAt(i);
        if (std::fprintf(fp, "S %s %s\n", st.roll().c_str(), st.name().c_str()) < 0) return false;
        for (int k = 0; k < st.regCount(); ++k) {
            const Registration& r = st.reg(k);
            std::string grade = r.hasGrade() ? r.grade() : std::string("-");
            if (std::fprintf(fp, "R %s %s %d %s %d\n", st.roll().c_str(),
                             db.courseAt(r.course()).code().c_str(), r.term(), grade.c_str(),
                             (int)r.thesisState()) < 0) {
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Reading: every record goes through the same validated AcadDB calls the
// interactive commands use, into a scratch database.
// ---------------------------------------------------------------------------
ErrorCode readProgram(AcadDB* db, const Words& w) {
    if (w.n != 6) return E_PARSE;
    int v[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 5; ++i) {
        if (!parseInt(w.w[i + 1], &v[i]) || v[i] < 0) return E_PARSE;
    }
    if (v[2] > 1000) return E_PARSE;
    Program p;
    p.totalCredits = v[0];
    p.electiveCredits = v[1];
    p.minGpaHundredths = v[2];
    p.maxLoad = v[3];
    p.thesisCredits = v[4];
    db->setProgram(p);
    return OK;
}

ErrorCode readCourse(AcadDB* db, const Words& w) {
    if (w.n != 6) return E_PARSE;
    if (idTooLong(w.w[1], MAX_CODE_LEN) || idTooLong(w.w[5], MAX_CODE_LEN)) return E_PARSE;
    CourseType type = REGULAR;
    bool core = true;
    int credits = 0;
    if (!parseCourseType(w.w[2], &type)) return E_PARSE;
    if (!parseInt(w.w[3], &credits)) return E_PARSE;
    if (!parseCategory(w.w[4], &core)) return E_PARSE;
    return db->addCourse(w.w[1], type, credits, core, w.w[5]);
}

ErrorCode readStudent(AcadDB* db, const Words& w) {
    if (w.n != 3) return E_PARSE;
    if (idTooLong(w.w[1], MAX_CODE_LEN) || idTooLong(w.w[2], MAX_NAME_LEN)) return E_PARSE;
    return db->addStudent(w.w[1], w.w[2]);
}

ErrorCode readRegistration(AcadDB* db, const Words& w) {
    if (w.n != 6) return E_PARSE;
    int term = 0;
    int state = 0;
    if (!parseTermNo(w.w[3], &term)) return E_PARSE;
    if (!parseInt(w.w[5], &state) || state < 0 || state > (int)FAILED) return E_PARSE;
    std::string grade = (w.w[4] == "-") ? std::string() : w.w[4];
    return db->restoreRegistration(w.w[1], w.w[2], term, grade, (ThesisState)state);
}

ErrorCode readRecord(AcadDB* db, const Words& w) {
    const std::string& tag = w.w[0];
    if (tag == "P") return readProgram(db, w);
    if (tag == "C") return readCourse(db, w);
    if (tag == "S") return readStudent(db, w);
    if (tag == "R") return readRegistration(db, w);
    return E_PARSE;
}

}  // namespace

ErrorCode saveDatabase(const AcadDB& db, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "w");
    if (fp == nullptr) return E_IO;
    bool ok = std::fprintf(fp, "ACADERP-LITE 1\n") > 0 && writeProgram(fp, db.program()) &&
              writeCourses(fp, db) && writeStudents(fp, db);
    if (std::fclose(fp) != 0) ok = false;
    return ok ? OK : E_IO;
}

// All or nothing: the file is read into a scratch database that replaces the
// live one only after every line was accepted.
ErrorCode loadDatabase(AcadDB* db, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "r");
    if (fp == nullptr) return E_IO;

    AcadDB scratch;
    ErrorCode ec = OK;
    bool sawHeader = false;
    std::string line;
    while (ec == OK && readLine(fp, &line)) {
        Words w;
        splitWords(line, &w);
        if (w.n == 0) continue;
        if (!sawHeader) {
            sawHeader = true;
            if (w.n != 2 || w.w[0] != "ACADERP-LITE" || w.w[1] != "1") ec = E_PARSE;
            continue;
        }
        ec = readRecord(&scratch, w);
    }
    std::fclose(fp);

    if (ec == OK && !sawHeader) ec = E_PARSE;
    if (ec == OK) db->swapContents(scratch);
    return ec;
}
