#include "builder.h"
#include "paths.h"
#include "runner.h"
#include "text.h"
#include "win32.h"
#include <vector>

static bool endsWithText(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string firstExisting(const std::vector<std::string>& candidates) {
    for (const std::string& candidate : candidates) {
        if (paths::fileExists(candidate))
            return candidate;
    }
    return "";
}

static std::string searchPath(const wchar_t* fileName) {
    wchar_t buffer[MAX_PATH];
    DWORD n = SearchPathW(nullptr, fileName, nullptr, MAX_PATH, buffer, nullptr);
    if (n == 0 || n >= MAX_PATH)
        return "";
    return utf8FromWide(buffer);
}

static void prependPath(const std::string& folder) {
    if (folder.empty() || !paths::folderExists(folder))
        return;
    wchar_t current[32768];
    DWORD n = GetEnvironmentVariableW(L"PATH", current, 32768);
    std::wstring path = wideFromUtf8(folder) + L";";
    if (n > 0 && n < 32768)
        path += current;
    SetEnvironmentVariableW(L"PATH", path.c_str());
}

static void addFoldersMissingFromExplorerPath() {
    wchar_t appData[MAX_PATH];
    wchar_t localAppData[MAX_PATH];
    wchar_t programFiles[MAX_PATH];
    GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
    prependPath(paths::join(utf8FromWide(programFiles), "nodejs"));
    prependPath(paths::join(utf8FromWide(appData), "npm"));
    prependPath(paths::join(utf8FromWide(localAppData), "pnpm"));
}

builder::ToolCheck builder::findTools() {
    ToolCheck check;
    check.nodePath = searchPath(L"node.exe");
    check.pnpmPath = firstExisting({
        searchPath(L"pnpm.cmd"),
        searchPath(L"pnpm.exe"),
        paths::join(paths::parentFolder(check.nodePath), "pnpm.cmd")
    });
    check.nodeFound = !check.nodePath.empty();
    check.pnpmFound = !check.pnpmPath.empty();

    if (!check.nodeFound) {
        check.userMessage = "Node.js wasn't found. Install Node.js LTS from nodejs.org, reopen this app, then try again.";
        return check;
    }
    if (!check.pnpmFound) {
        check.userMessage = "pnpm wasn't found. In a terminal run  npm i -g pnpm  then reopen this app.";
        return check;
    }
    return check;
}

static Outcome runPnpm(
    const std::string& equicordRoot,
    const std::wstring& args,
    const std::function<void(const std::string&)>& onLine,
    std::string* capturedOutput = nullptr
) {
    addFoldersMissingFromExplorerPath();
    builder::ToolCheck tools = builder::findTools();
    if (!tools.nodeFound || !tools.pnpmFound)
        return fail(tools.userMessage);

    std::wstring command = L"cmd.exe /C \"\"";
    command += wideFromUtf8(tools.pnpmPath);
    command += L"\" ";
    command += args;
    command += L"\"";

    runner::RunResult ran = runner::runCaptured(command, equicordRoot, onLine);
    if (capturedOutput)
        *capturedOutput = ran.output;
    if (!ran.started)
        return fail(ran.userMessage, ran.output);
    if (ran.exitCode != 0) {
        std::string detail = ran.output;
        if (detail.empty()) detail = "No output was captured. Try running 'pnpm build' manually in the Equicord folder.";
        return fail(
            "Equicord build failed.",
            detail
        );
    }
    return ok();
}

static std::string injectFailureFromOutput(const std::string& output) {
    struct Marker { const char* text; const char* message; };
    const Marker markers[] = {
        { "Cannot patch because Discord's files are used by a different process",
          "Discord is still running, so its files are locked. Quit it from the tray, then click Install again." },
        { "No Discord install found",
          "The installer couldn't find that Discord build. Pick a different one in Settings." },
        { "flag provided but not defined",
          "The installer rejected the options it was given. Your Equicord copy may be newer than this app expects." },
        { "flags are mutually exclusive",
          "The installer rejected the options it was given. Your Equicord copy may be newer than this app expects." },
        { "Something went wrong. Please check the logs above.",
          "The installer failed to patch Discord. Quit Discord fully from the tray, then click Install again." }
    };
    for (const Marker& marker : markers) {
        if (output.find(marker.text) != std::string::npos)
            return marker.message;
    }
    return "";
}

Outcome builder::checkUserplugins(const std::string& equicordRoot) {
    std::string root = paths::userPluginsFolder(equicordRoot);
    if (!paths::folderExists(root))
        return ok();

    paths::cleanWorkFolders(root);

    const char* archiveParts[] = { ".zip", ".rar", ".7z" };
    std::vector<std::string> broken;

    for (const paths::DirEntry& entry : paths::listEntries(root)) {
        const std::string& name = entry.name;
        if (name.empty() || name[0] == '_' || name[0] == '.')
            continue;
        if (name == "index.ts")
            continue;

        std::string lower = toLowerAscii(name);
        bool isArchive = false;
        for (const char* part : archiveParts) {
            if (lower.find(part) != std::string::npos) {
                isArchive = true;
                break;
            }
        }
        if (isArchive)
            continue;

        if (entry.isFolder) {
            std::string folder = paths::join(root, name);
            if (paths::hasPluginIndex(folder) || paths::fileExists(paths::join(folder, "index.js")))
                continue;
            broken.push_back(name + "  (folder has no index.ts or index.tsx)");
            continue;
        }

        if (!endsWithText(lower, ".ts") && !endsWithText(lower, ".tsx")
            && !endsWithText(lower, ".js") && !endsWithText(lower, ".jsx"))
            broken.push_back(name + "  (not a plugin source file)");
    }

    if (broken.empty())
        return ok();

    std::string detail;
    for (const std::string& item : broken)
        detail += "src/userplugins/" + item + "\n";

    return fail(
        "src/userplugins has " + std::to_string(broken.size())
        + " entry(s) Equicord cannot build. Give each plugin folder an index.tsx, or remove it, then try again.",
        detail
    );
}

Outcome builder::ensureDependencies(const std::string& equicordRoot, const std::function<void(const std::string& line)>& onLine) {
    if (paths::folderExists(paths::join(equicordRoot, "node_modules")))
        return ok();
    if (onLine)
        onLine("Installing Equicord dependencies (pnpm install)...");
    return runPnpm(equicordRoot, L"install --no-frozen-lockfile", onLine);
}

Outcome builder::build(const std::string& equicordRoot, bool devMode, const std::function<void(const std::string& line)>& onLine) {
    Outcome usable = checkUserplugins(equicordRoot);
    if (!usable.succeeded) {
        if (onLine) {
            onLine(usable.userMessage);
            onLine(usable.detail);
        }
        return usable;
    }

    Outcome deps = ensureDependencies(equicordRoot, onLine);
    if (!deps.succeeded)
        return deps;
    std::wstring args = devMode ? L"build --dev" : L"build";
    if (onLine)
        onLine(devMode ? "Running pnpm build --dev..." : "Running pnpm build...");
    Outcome built = runPnpm(equicordRoot, args, onLine);
    if (!built.succeeded)
        return built;
    return ok("Equicord built.");
}

static std::string localAppData() {
    wchar_t folder[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", folder, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return "";
    return utf8FromWide(folder);
}

static bool installIsPatched(const std::string& installFolder) {
    for (const std::string& child : paths::listSubfolders(installFolder)) {
        if (child.compare(0, 4, "app-") != 0)
            continue;
        std::string resources = paths::join(paths::join(installFolder, child), "resources");
        if (paths::fileExists(paths::join(resources, "_app.asar")))
            return true;
    }
    return false;
}

static Outcome repairStalePatch(
    const std::string& installFolder,
    const std::string& label,
    const std::function<void(const std::string&)>& onLine
) {
    for (const std::string& child : paths::listSubfolders(installFolder)) {
        if (child.compare(0, 4, "app-") != 0)
            continue;

        std::string resources = paths::join(paths::join(installFolder, child), "resources");
        std::string asar = paths::join(resources, "app.asar");
        std::string original = paths::join(resources, "_app.asar");

        if (!paths::folderExists(asar) || !paths::fileExists(original))
            continue;

        if (onLine)
            onLine("Found an old-style patch on " + label + " that the installer can't undo. Restoring the original...");

        Outcome cleared = paths::deleteFolderIfSafe(asar, resources);
        if (!cleared.succeeded || paths::folderExists(asar)) {
            return fail(
                "Couldn't clear an old patch from " + label + " because its files are still in use. "
                "Quit it from the tray icon, then click Install again.",
                asar
            );
        }

        if (!MoveFileW(wideFromUtf8(original).c_str(), wideFromUtf8(asar).c_str())) {
            return fail(
                "Couldn't put " + label + "'s original app.asar back, so it has been left unpatched. "
                "Reinstall that Discord build, then try again.",
                "MoveFile " + std::to_string(GetLastError())
            );
        }

        if (onLine)
            onLine("Restored the original app.asar.");
    }
    return ok();
}

std::vector<builder::DiscordChannel> builder::discordChannels() {
    std::string root = localAppData();
    DiscordChannel channels[] = {
        { "stable", "Discord Stable", "Discord", false },
        { "ptb", "Discord PTB", "DiscordPTB", false },
        { "canary", "Discord Canary", "DiscordCanary", false },
        { "development", "Discord Development", "DiscordDevelopment", false },
    };
    std::vector<DiscordChannel> found;
    for (DiscordChannel channel : channels) {
        channel.folder = paths::join(root, channel.folder);
        channel.installed = paths::folderExists(channel.folder);
        channel.patched = channel.installed && installIsPatched(channel.folder);
        found.push_back(channel);
    }
    return found;
}

Outcome builder::inject(const std::string& equicordRoot, const std::string& branch, const std::function<void(const std::string& line)>& onLine) {
    std::wstring args;
    std::string label = branch;
    std::string installFolder;
    if (branch == "development") {
        std::string location;
        for (const DiscordChannel& channel : discordChannels()) {
            if (channel.branch == "development")
                location = channel.folder;
        }
        if (location.empty() || !paths::folderExists(location)) {
            return fail(
                "Discord Development isn't installed. Install that Discord build, or pick Stable, PTB, or Canary."
            );
        }
        args = L"inject --location \"" + wideFromUtf8(location) + L"\"";
        label = "Discord Development";
        installFolder = location;
    } else {
        std::string selected = branch;
        if (selected != "stable" && selected != "ptb" && selected != "canary")
            selected = "stable";
        args = L"inject --branch " + wideFromUtf8(selected);
        for (const DiscordChannel& channel : discordChannels()) {
            if (channel.branch == selected)
                installFolder = channel.folder;
        }
        if (selected == "ptb")
            label = "Discord PTB";
        else if (selected == "canary")
            label = "Discord Canary";
        else
            label = "Discord Stable";
    }
    if (!installFolder.empty()) {
        Outcome repaired = repairStalePatch(installFolder, label, onLine);
        if (!repaired.succeeded)
            return repaired;
    }

    if (onLine)
        onLine("Injecting Equicord into " + label + "...");
    std::string output;
    Outcome injected = runPnpm(equicordRoot, args, onLine, &output);

    std::string problem = injectFailureFromOutput(output);
    if (!problem.empty())
        return fail(problem, output);

    if (!injected.succeeded) {
        injected.userMessage =
            "Equicord built, but it couldn't inject into " + label
            + ". Close it fully from the tray, then click Install again.";
        return injected;
    }
    return ok("Successfully instaleld to " + label + ". Settings > Plugins > User Plugins to enable it.");
}
