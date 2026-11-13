// main.cpp - reads commands from standard input, one per line.
// A failing command prints "ERROR <line> <CODE>" and the loop carries on;
// blank lines and '#' comments still advance the line counter.
#include "erp.h"

int main() {
    Session session;
    std::string line;
    int lineNo = 0;
    while (std::getline(std::cin, line)) {
        ++lineNo;
        ErrorCode ec = session.execute(line);
        if (ec != OK) {
            std::cout << "ERROR " << lineNo << ' ' << errorName(ec) << '\n';
        }
    }
    return 0;
}
