#include "log.h"
#include "win32.h"
#include <cstdio>
#include <mutex>
#include <vector>

static std::mutex logMutex;
static std::vector<std::string> lines;
static std::function<void()> listener;

static SYSTEMTIME nowLocal() {
    SYSTEMTIME time;
    GetLocalTime(&time);
    return time;
}

static std::string stamp() {
    SYSTEMTIME time = nowLocal();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

void logLine(const char* tag, const char* message) {
    std::string line = "[" + stamp() + "] [" + tag + "] " + message;
    {
        std::lock_guard<std::mutex> lock(logMutex);
        lines.push_back(line);
        if (lines.size() > 800)
            lines.erase(lines.begin(), lines.begin() + 200);
    }
    if (listener)
        listener();
}

void logLine(const std::string& tag, const std::string& message) {
    logLine(tag.c_str(), message.c_str());
}

std::string appLog::text() {
    std::lock_guard<std::mutex> lock(logMutex);
    std::string out;
    for (const std::string& line : lines) {
        out += line;
        out += "\r\n";
    }
    return out;
}

void appLog::setListener(const std::function<void()>& onChange) {
    listener = onChange;
}
