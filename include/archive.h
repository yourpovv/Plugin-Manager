#pragma once

#include "outcome.h"
#include <string>

namespace archive {

bool isArchivePath(const std::string& pathOrUrl);
bool isArchiveBytes(const std::string& body);
Outcome extractArchive(const std::string& archivePath, const std::string& destFolder);
std::string unwrapGithubFolder(const std::string& extractedRoot);
std::string locatePluginSource(const std::string& extractedRoot, const std::string& sourcePath);

}
