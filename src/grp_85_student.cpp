// student.cpp - Registration and Student.
#include "erp.h"

void Registration::open(int course, int term, ThesisState initial) {
    course_ = course;
    term_ = term;
    grade_.clear();
    state_ = initial;
}

// The registration table is the only heap array in a Student. It is created
// here and released in the destructor, exactly once per student.
Student::Student(const std::string& roll, const std::string& name)
    : roll_(roll), name_(name), regs_(new Registration[MAX_REGS]), numRegs_(0) {}

Student::~Student() {
    delete[] regs_;
}

ErrorCode Student::addRegistration(int course, int term, ThesisState initial) {
    if (numRegs_ >= MAX_REGS) return E_FULL;
    regs_[numRegs_].open(course, term, initial);
    ++numRegs_;
    return OK;
}

int Student::latestFor(int course, int term) const {
    for (int i = numRegs_ - 1; i >= 0; --i) {
        if (regs_[i].course() != course) continue;
        if (term == 0 || regs_[i].term() == term) return i;
    }
    return -1;
}
