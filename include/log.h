#pragma once

#include <functional>
#include <string>

void logLine(const char* tag, const char* message);
void logLine(const std::string& tag, const std::string& message);

namespace appLog {
    std::string text();
    void setListener(const std::function<void()>& onChange);
}
