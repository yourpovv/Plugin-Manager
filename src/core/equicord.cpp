#include "equicord.h"
#include "json.h"
#include "paths.h"
#include "text.h"
#include "win32.h"
#include <shlobj.h>
#include <vector>

static std::string readPackageName(const std::string& packageJson) {
    std::string raw = paths::readFile(packageJson);
    if (raw.empty())
        return "";
    return toLowerAscii(json::getString(raw, "name"));
}

equicord::FolderCheck equicord::validate(const std::string& folder) {
    FolderCheck check;
    check.path = paths::canonicalize(folder);
    if (check.path.empty() || !paths::folderExists(check.path)) {
        check.userMessage = "That Equicord folder doesn't exist. Click Browse and pick the folder you cloned Equicord into.";
        return check;
    }

    std::string packageJson = paths::join(check.path, "package.json");
    if (!paths::fileExists(packageJson)) {
        check.userMessage = "That folder isn't Equicord source. Pick the folder that contains package.json, src, and scripts.";
        return check;
    }

    std::string name = readPackageName(packageJson);
    if (name != "equicord") {
        check.userMessage = "That folder's package.json is not Equicord. Pick the Equicord source folder, not Discord itself.";
        return check;
    }

    if (!paths::folderExists(paths::join(check.path, "src"))) {
        check.userMessage = "That Equicord folder is missing src. Re-clone Equicord, then pick the new folder.";
        return check;
    }

    std::string buildScript = paths::join(paths::join(paths::join(check.path, "scripts"), "build"), "build.mjs");
    if (!paths::fileExists(buildScript)) {
        check.userMessage = "That Equicord folder is missing scripts/build/build.mjs, so this app can't rebuild it. Use a full source clone.";
        return check;
    }

    check.isValid = true;
    check.userMessage = "Equicord source is valid.";
    return check;
}

static std::string firstValid(const std::vector<std::string>& candidates) {
    for (const std::string& candidate : candidates) {
        if (equicord::validate(candidate).isValid)
            return paths::canonicalize(candidate);
    }
    return "";
}

std::string equicord::guessInstallFolder() {
    wchar_t profile[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, profile)))
        return "";
    std::string home = utf8FromWide(profile);
    std::string exe = paths::exeFolder();
    std::string exeParent = paths::parentFolder(exe);
    std::string exeGrand = paths::parentFolder(exeParent);
    std::string exeGreat = paths::parentFolder(exeGrand);
    return firstValid({
        paths::join(exeParent, "Equicord"),
        paths::join(exeGrand, "Equicord"),
        paths::join(exeGreat, "Equicord"),
        paths::join(exeParent, "..\\Equicord"),
        paths::join(exeGrand, "..\\Equicord"),
        paths::join(exeGrand, "Equicord\\Equicord"),
        paths::join(home, "Equicord"),
        paths::join(home, "Documents\\Equicord"),
        paths::join(home, "Desktop\\Equicord"),
        paths::join(home, "Downloads\\Equicord"),
        paths::join(home, "Documents\\GitHub\\Equicord"),
        paths::join(home, "source\\repos\\Equicord"),
        "C:\\Equicord",
        "D:\\Equicord",
        "D:\\- File Backup\\Files\\Discord\\Clients\\Equicord\\Equicord",
        "D:\\- File Backup\\Files\\Discord\\Clients\\Equicord\\Plugins\\Equicord"
    });
}
