#pragma once

#include "outcome.h"
#include <functional>
#include <string>
#include <vector>

struct PluginFile {
    std::string url;
    std::string relativePath;
};

struct CatalogPlugin {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::string downloadUrl;
    std::string sourcePath;
    std::vector<PluginFile> files;
};

struct InstalledPlugin {
    std::string id;
    std::string version;
    bool hasIndex = false;
};

struct PluginStatus {
    CatalogPlugin catalog;
    bool isInstalled = false;
    bool updateAvailable = false;
    std::string installedVersion;
};

namespace plugins {

std::vector<InstalledPlugin> scanInstalled(const std::string& equicordRoot);
std::vector<PluginStatus> mergeStatus(const std::vector<CatalogPlugin>& catalog, const std::string& equicordRoot);
bool versionIsNewer(const std::string& remoteVersion, const std::string& localVersion);
CatalogPlugin fromSource(const std::string& source);

Outcome install(
    const CatalogPlugin& plugin,
    const std::string& equicordRoot,
    const std::function<void(const std::string& status)>& progress
);

Outcome removePlugin(const std::string& pluginId, const std::string& equicordRoot);

}
