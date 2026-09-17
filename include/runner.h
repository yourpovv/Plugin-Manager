#pragma once

#include "outcome.h"
#include <functional>
#include <string>

namespace runner {

struct RunResult {
    bool started = false;
    unsigned long exitCode = (unsigned long)-1;
    std::string output;
    std::string userMessage;
};

RunResult runCaptured(
    const std::wstring& commandLine,
    const std::string& workingFolder,
    const std::function<void(const std::string& line)>& onLine
);

}
