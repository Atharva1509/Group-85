# AcadERP-Lite - build file for the src/ directory.
# Usage:  make        builds ./acaderp
#         make clean  removes objects and the binary
CXX      = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -fno-exceptions -O2
SRCS     = main.cpp commands.cpp db.cpp course.cpp student.cpp grading.cpp parse.cpp persist.cpp
OBJS     = $(SRCS:.cpp=.o)

all: acaderp

acaderp: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp erp.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) acaderp

.PHONY: all clean
