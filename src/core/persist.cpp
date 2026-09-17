#include "persist.h"
#include "config.h"
#include "json.h"
#include "paths.h"
#include "text.h"
#include "win32.h"
#include <cstdio>
#include <fstream>
#include <shlobj.h>

std::string persist::settingsPath() {
    wchar_t appData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        return paths::join(paths::exeFolder(), "settings.json");
    std::string folder = paths::join(utf8FromWide(appData), "EquicordPluginManager");
    return paths::join(folder, "settings.json");
}

persist::Settings persist::load() {
    Settings settings;
    settings.manifestUrl = cfg::manifestUrl;
    settings.autoBuild = true;
    settings.buildDev = false;
    settings.discordBranch = "stable";

    std::string raw = paths::readFile(settingsPath());
    if (raw.empty())
        return settings;

    settings.equicordPath = json::getString(raw, "equicordPath");
    std::string manifestUrl = json::getString(raw, "manifestUrl");
    if (!manifestUrl.empty())
        settings.manifestUrl = manifestUrl;
    settings.autoBuild = json::getBool(raw, "autoBuild", true);
    settings.buildDev = json::getBool(raw, "buildDev", false);
    std::string branch = json::getString(raw, "discordBranch");
    if (branch == "stable" || branch == "ptb" || branch == "canary" || branch == "development")
        settings.discordBranch = branch;
    return settings;
}

void persist::save(const Settings& settings) {
    paths::ensureFolder(paths::parentFolder(settingsPath()));
    char buffer[4096];
    snprintf(
        buffer,
        sizeof(buffer),
        "{\n  \"equicordPath\": \"%s\",\n  \"manifestUrl\": \"%s\",\n  \"autoBuild\": %s,\n  \"buildDev\": %s,\n  \"discordBranch\": \"%s\"\n}\n",
        json::escape(settings.equicordPath).c_str(),
        json::escape(settings.manifestUrl).c_str(),
        settings.autoBuild ? "true" : "false",
        settings.buildDev ? "true" : "false",
        json::escape(settings.discordBranch).c_str()
    );
    std::string path = settingsPath();
    std::ofstream file(wideFromUtf8(path).c_str(), std::ios::binary);
    if (file)
        file << buffer;
}
