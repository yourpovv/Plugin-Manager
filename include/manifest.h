#pragma once

#include "plugin.h"
#include <string>
#include <vector>

namespace manifest {

struct Catalog {
    bool loaded = false;
    int version = 0;
    std::vector<CatalogPlugin> plugins;
    std::string source;
    std::string userMessage;
    std::string detail;
};

Catalog parse(const std::string& jsonText, const std::string& sourceLabel);
Catalog loadFromUrlOrFile(const std::string& urlOrPath);

}
