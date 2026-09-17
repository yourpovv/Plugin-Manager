#pragma once

#include <string>

namespace persist {

struct Settings {
    std::string equicordPath;
    std::string manifestUrl;
    bool autoBuild = true;
    bool buildDev = false;
    std::string discordBranch = "stable";
};

Settings load();
void save(const Settings& settings);
std::string settingsPath();

}
