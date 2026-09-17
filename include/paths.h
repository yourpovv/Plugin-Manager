#pragma once

#include "outcome.h"
#include <functional>
#include <string>
#include <vector>

namespace paths {

bool isSafePluginId(const std::string& pluginId);
bool isSafeRelativeFile(const std::string& relativePath);
bool isSafeArchiveEntry(const std::string& entry);
bool isPluginSourceFile(const std::string& path);

std::string join(const std::string& left, const std::string& right);
std::string canonicalize(const std::string& path);
bool isInside(const std::string& child, const std::string& parent);

bool fileExists(const std::string& path);
bool folderExists(const std::string& path);
Outcome ensureFolder(const std::string& path);
Outcome deleteFolderIfSafe(const std::string& folder, const std::string& allowedRoot);
Outcome copyFileIfSafe(const std::string& source, const std::string& dest, const std::string& allowedRoot);
Outcome copyTreeIfSafe(const std::string& sourceFolder, const std::string& destFolder, const std::string& allowedRoot);
Outcome replaceFolderIfSafe(const std::string& stagingFolder, const std::string& finalFolder, const std::string& allowedRoot);

std::string parentFolder(const std::string& path);
std::string fileName(const std::string& path);
std::string exeFolder();
std::string ensureTempFolder();
std::string userPluginsFolder(const std::string& equicordRoot);
std::string pluginFolder(const std::string& equicordRoot, const std::string& pluginId);

std::string readFile(const std::string& path);
Outcome writeFileIfSafe(const std::string& path, const std::string& contents, const std::string& allowedRoot);

struct DirEntry {
    std::string name;
    bool isFolder = false;
};

std::vector<std::string> listSubfolders(const std::string& folder);
std::vector<DirEntry> listEntries(const std::string& folder);
Outcome cleanWorkFolders(const std::string& userPluginsRoot);
bool hasPluginIndex(const std::string& folder);

void walkFiles(const std::string& folder, const std::function<void(const std::string& relative, const std::string& full)>& visit);

}
