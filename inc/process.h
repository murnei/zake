#pragma once

#include <string>
#include <vector>

struct ProcessResult {
    int exit_code;
    std::string output;
};

ProcessResult run_process(const std::vector<std::string>& argv);
