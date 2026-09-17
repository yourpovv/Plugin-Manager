#pragma once

#include "outcome.h"
#include <functional>
#include <string>
#include <vector>

namespace builder {

struct ToolCheck {
    bool nodeFound = false;
    bool pnpmFound = false;
    std::string nodePath;
    std::string pnpmPath;
    std::string userMessage;
};

struct DiscordChannel {
    std::string branch;
    std::string label;
    std::string folder;
    bool installed = false;
    bool patched = false;
};

ToolCheck findTools();
Outcome checkUserplugins(const std::string& equicordRoot);
std::vector<DiscordChannel> discordChannels();
Outcome ensureDependencies(const std::string& equicordRoot, const std::function<void(const std::string& line)>& onLine);
Outcome build(const std::string& equicordRoot, bool devMode, const std::function<void(const std::string& line)>& onLine);
Outcome inject(const std::string& equicordRoot, const std::string& branch, const std::function<void(const std::string& line)>& onLine);

}
