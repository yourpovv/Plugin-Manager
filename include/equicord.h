#pragma once

#include <string>

namespace equicord {

struct FolderCheck {
    bool isValid = false;
    std::string path;
    std::string userMessage;
};

FolderCheck validate(const std::string& folder);
std::string guessInstallFolder();

}
