#include "helper.h"

#include <iostream>

#include "error.h"

std::string read_line() {
    std::string line;
    if (!std::getline(std::cin, line))
        throw ZakeError(ErrorCode::InputClosed);

    size_t b = line.find_first_not_of(" \t\r");
    if (b == std::string::npos)
        return {};
    size_t e = line.find_last_not_of(" \t\r");
    return line.substr(b, e - b + 1);
}

int read_int() {
    std::string s = read_line();
    size_t pos = 0;
    int value = 0;

    try {
        value = std::stoi(s, &pos);
    } catch (...) {
        throw ZakeError(ErrorCode::InvalidInput, "expected a number, got '" + s + "'");
    }

    if (pos != s.size())
        throw ZakeError(ErrorCode::InvalidInput, "expected a number, got '" + s + "'");

    return value;
}

void wait_enter() {
    std::cout << "\nPress Enter to continue..." << std::flush;
    std::string dummy;
    std::getline(std::cin, dummy);
}
