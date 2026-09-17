#pragma once

#include <string>

struct Outcome {
    bool succeeded = false;
    std::string userMessage;
    std::string detail;
};

inline Outcome ok(const std::string& userMessage = "") {
    return Outcome{true, userMessage, ""};
}

inline Outcome fail(const std::string& userMessage, const std::string& detail = "") {
    return Outcome{false, userMessage, detail};
}
