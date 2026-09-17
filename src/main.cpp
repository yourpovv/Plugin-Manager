#include "builder.h"
#include "config.h"
#include "equicord.h"
#include "log.h"
#include "manifest.h"
#include "paths.h"
#include "persist.h"
#include "plugin.h"
#include "text.h"
#include "window.h"
#include <atomic>
#include <mutex>
#include <process.h>
#include <shobjidl.h>
#include <string>
#include <thread>
#include <vector>

static std::mutex catalogMutex;
static std::vector<PluginStatus> catalogStatus;
static std::atomic<bool> jobRunning{false};

static std::vector<CatalogPlugin> catalogItems() {
    std::lock_guard<std::mutex> lock(catalogMutex);
    std::vector<CatalogPlugin> items;
    for (const PluginStatus& status : catalogStatus)
        items.push_back(status.catalog);
    return items;
}

static void saveCfg() {
    persist::Settings settings;
    settings.equicordPath = cfg::equicordPath;
    settings.manifestUrl = cfg::manifestUrl;
    settings.autoBuild = cfg::autoBuild;
    settings.buildDev = cfg::buildDev;
    settings.discordBranch = cfg::discordBranch;
    persist::save(settings);
}

static CatalogPlugin pluginById(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(catalogMutex);
    for (const PluginStatus& status : catalogStatus) {
        if (status.catalog.id == pluginId)
            return status.catalog;
    }
    return CatalogPlugin{};
}

static void replaceCatalog(const std::vector<CatalogPlugin>& plugins) {
    std::vector<PluginStatus> merged = plugins::mergeStatus(plugins, cfg::equicordPath);
    {
        std::lock_guard<std::mutex> lock(catalogMutex);
        catalogStatus = merged;
    }
    ui::setCatalog(merged);
}

static void applyEquicordPath(const std::string& folder) {
    equicord::FolderCheck check = equicord::validate(folder);
    cfg::equicordPath = check.isValid ? check.path : folder;
    saveCfg();
    ui::setEquicordPath(cfg::equicordPath, check.isValid, check.userMessage);
    logLine(check.isValid ? "OK" : "ERROR", check.userMessage);
    if (check.isValid)
        replaceCatalog(catalogItems());
}

template<typename Interface>
struct ComRelease {
    Interface* ptr = nullptr;
    ~ComRelease() {
        if (ptr)
            ptr->Release();
    }
    Interface* operator->() { return ptr; }
    bool valid() const { return ptr != nullptr; }
};

static std::string pickEquicordFolder() {
    ComRelease<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog.ptr))))
        return "";
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Select your Equicord folder");
    if (FAILED(dialog->Show(ui::mainWindow())))
        return "";

    ComRelease<IShellItem> item;
    if (FAILED(dialog->GetResult(&item.ptr)))
        return "";
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return "";
    std::string chosen = utf8FromWide(path);
    CoTaskMemFree(path);
    return chosen;
}

static void refreshCatalog() {
    if (cfg::manifestUrl.empty()) {
        logLine("INFO", "No personal plugins.json set. Install from a GitHub link, folder, zip, or drop.");
        replaceCatalog({});
        ui::toast("Paste a link or drop a plugin to install it.", false);
        return;
    }

    logLine("INFO", "Loading personal plugins from " + cfg::manifestUrl);
    manifest::Catalog catalog = manifest::loadFromUrlOrFile(cfg::manifestUrl);
    if (!catalog.loaded) {
        logLine("INFO", catalog.userMessage);
        replaceCatalog({});
        ui::toast("Personal list not loaded. You can still install from a link or drop a plugin.", false);
        return;
    }

    replaceCatalog(catalog.plugins);
    logLine("OK", "Loaded " + std::to_string(catalog.plugins.size()) + " personal plugin(s).");
    ui::toast("Install from a link, a local path, or your personal list.", false);
}

static void logBuildLine(const std::string& line) {
    if (!line.empty())
        logLine("BUILD", line);
}

static Outcome runBuild() {
    ui::setBusy(true, "Building Equicord...");
    logLine("INFO", "Starting Equicord build.");
    Outcome built = builder::build(cfg::equicordPath, cfg::buildDev, logBuildLine);
    if (!built.succeeded) {
        logLine("ERROR", built.userMessage);
        return built;
    }
    logLine("OK", built.userMessage);
    Outcome injected = builder::inject(cfg::equicordPath, cfg::discordBranch, logBuildLine);
    if (injected.succeeded)
        logLine("OK", injected.userMessage);
    else
        logLine("ERROR", injected.userMessage);
    return injected;
}

static std::string workBlocker() {
    if (jobRunning)
        return "Wait for the current install or build to finish.";
    equicord::FolderCheck check = equicord::validate(cfg::equicordPath);
    if (!check.isValid)
        return check.userMessage;
    return "";
}

static std::vector<std::string> splitIds(const std::string& packed) {
    std::vector<std::string> ids;
    size_t start = 0;
    while (start < packed.size()) {
        size_t comma = packed.find(',', start);
        if (comma == std::string::npos)
            comma = packed.size();
        std::string id = packed.substr(start, comma - start);
        if (!id.empty())
            ids.push_back(id);
        start = comma + 1;
    }
    return ids;
}

static void startInstallPlugins(std::vector<CatalogPlugin> toInstall) {
    std::string blocker = workBlocker();
    if (!blocker.empty()) {
        ui::toast(blocker, true);
        return;
    }
    if (toInstall.empty()) {
        ui::toast("Nothing to install. Paste a link or drop a plugin folder.", true);
        return;
    }

    jobRunning = true;
    std::thread([toInstall]() {
        ui::setBusy(true, "Installing plugins...");
        bool anyInstalled = false;
        Outcome lastFail = ok();
        for (size_t i = 0; i < toInstall.size(); i++) {
            const CatalogPlugin& plugin = toInstall[i];
            ui::setProgress((int)((i * 100) / toInstall.size()));
            logLine("INFO", "Installing " + plugin.name);
            Outcome installed = plugins::install(plugin, cfg::equicordPath, [](const std::string& status) {
                logLine("INFO", status);
            });
            if (installed.succeeded) {
                anyInstalled = true;
                logLine("OK", installed.userMessage);
            } else {
                lastFail = installed;
                logLine("ERROR", installed.userMessage);
                if (!installed.detail.empty())
                    logLine("ERROR", installed.detail);
            }
        }

        Outcome built = ok();
        if (anyInstalled && cfg::autoBuild)
            built = runBuild();

        replaceCatalog(catalogItems());

        if (!lastFail.succeeded && !anyInstalled)
            ui::toast(lastFail.userMessage, true);
        else if (!built.succeeded)
            ui::toast(built.userMessage, true);
        else if (anyInstalled && cfg::autoBuild)
            ui::toast(built.userMessage, false);
        else if (anyInstalled)
            ui::toast("Plugins installed. Click Build to inject into Discord, then restart it.", false);

        ui::setBusy(false, "");
        jobRunning = false;
    }).detach();
}

static void startInstall(const std::vector<std::string>& ids) {
    std::vector<CatalogPlugin> toInstall;
    for (const std::string& id : ids) {
        CatalogPlugin plugin = pluginById(id);
        if (plugin.id.empty() || plugin.downloadUrl.empty()) {
            ui::toast(id + " is already on disk. Use a GitHub link or local path to install or update it.", true);
            return;
        }
        toInstall.push_back(plugin);
    }
    startInstallPlugins(toInstall);
}

static void startInstallFromSource(const std::string& source) {
    std::string trimmed = source;
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
        trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\r' || trimmed.back() == '\n'))
        trimmed.pop_back();
    if (trimmed.empty()) {
        ui::toast("Paste a GitHub folder link, or a local folder/zip path.", true);
        return;
    }
    CatalogPlugin plugin = plugins::fromSource(trimmed);
    logLine("INFO", "Installing " + plugin.id + " from " + trimmed);
    startInstallPlugins({ plugin });
}

static std::string pickPluginFile() {
    int choice = MessageBoxW(ui::mainWindow(), L"Pick a plugin folder (like C:\\Users\\pov\\Desktop\\streamProof) or a file/archive (.ts/.tsx/.css/.zip/.rar/.7z)?\n\nYes = Pick File/Archive\nNo = Pick Folder", L"Select plugin source", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL)
        return "";
    bool pickFolder = (choice == IDNO);
    ComRelease<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog.ptr))))
        return "";
    if (!pickFolder) {
        COMDLG_FILTERSPEC filters[] = {
            { L"Equicord plugin", L"*.ts;*.tsx;*.css;*.zip;*.rar;*.7z;*.tar;*.gz;*.tgz" },
            { L"All files", L"*.*" }
        };
        dialog->SetFileTypes(2, filters);
        dialog->SetTitle(L"Select plugin file or archive");
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions((options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST) & ~FOS_PICKFOLDERS);
    } else {
        dialog->SetTitle(L"Select plugin folder (must contain index.ts or index.tsx)");
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    if (FAILED(dialog->Show(ui::mainWindow())))
        return "";
    ComRelease<IShellItem> item;
    if (FAILED(dialog->GetResult(&item.ptr)))
        return "";
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return "";
    std::string chosen = utf8FromWide(path);
    CoTaskMemFree(path);
    return chosen;
}

static void startRemove(const std::string& pluginId) {
    std::string blocker = workBlocker();
    if (!blocker.empty()) {
        ui::toast(blocker, true);
        return;
    }
    jobRunning = true;
    std::thread([pluginId]() {
        ui::setBusy(true, "Removing plugin...");
        Outcome removed = plugins::removePlugin(pluginId, cfg::equicordPath);
        if (removed.succeeded)
            logLine("OK", removed.userMessage);
        else
            logLine("ERROR", removed.userMessage);

        Outcome built = ok();
        if (removed.succeeded && cfg::autoBuild)
            built = runBuild();

        replaceCatalog(catalogItems());

        if (!removed.succeeded)
            ui::toast(removed.userMessage, true);
        else if (!built.succeeded)
            ui::toast(built.userMessage, true);
        else if (cfg::autoBuild)
            ui::toast(built.userMessage, false);
        else
            ui::toast("Plugin removed. Click Build, then restart Discord.", false);

        ui::setBusy(false, "");
        jobRunning = false;
    }).detach();
}

static void startBuild() {
    std::string blocker = workBlocker();
    if (!blocker.empty()) {
        ui::toast(blocker, true);
        return;
    }
    jobRunning = true;
    std::thread([]() {
        Outcome built = runBuild();
        ui::toast(built.userMessage, !built.succeeded);
        ui::setBusy(false, "");
        jobRunning = false;
    }).detach();
}

static void applySettings(const std::string& packed) {
    size_t newline = packed.find('\n');
    std::string url = packed;
    std::string flags;
    if (newline != std::string::npos) {
        url = packed.substr(0, newline);
        flags = packed.substr(newline + 1);
    }
    while (!url.empty() && (url.back() == ' ' || url.back() == '\r'))
        url.pop_back();
    if (!url.empty())
        cfg::manifestUrl = url;
    if (flags.size() >= 1)
        cfg::autoBuild = flags[0] == '1';
    if (flags.size() >= 2)
        cfg::buildDev = flags[1] == '1';
    saveCfg();
    logLine("OK", "Settings saved.");
    refreshCatalog();
}

static void handleCommand(const std::string& command) {
    if (command == "browse") {
        std::string folder = pickEquicordFolder();
        if (!folder.empty())
            applyEquicordPath(folder);
        return;
    }
    if (command == "browse-plugin") {
        std::string file = pickPluginFile();
        if (file.empty())
            return;
        ui::setSourceText(file);
        startInstallFromSource(file);
        return;
    }
    if (startsWith(command, "install-source:")) {
        startInstallFromSource(command.substr(15));
        return;
    }
    if (command == "refresh") {
        refreshCatalog();
        return;
    }
    if (command == "rebuild") {
        startBuild();
        return;
    }
    if (startsWith(command, "install:")) {
        startInstall(splitIds(command.substr(8)));
        return;
    }
    if (startsWith(command, "remove:")) {
        startRemove(command.substr(7));
        return;
    }
    if (startsWith(command, "settings:")) {
        applySettings(command.substr(9));
        return;
    }
    if (startsWith(command, "discord-branch:")) {
        std::string branch = command.substr(15);
        if (branch == "stable" || branch == "ptb" || branch == "canary" || branch == "development") {
            cfg::discordBranch = branch;
            saveCfg();
            logLine("OK", "Discord target set to " + branch + ".");
        }
        return;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    persist::Settings settings = persist::load();
    cfg::equicordPath = settings.equicordPath;
    cfg::manifestUrl = settings.manifestUrl;
    cfg::autoBuild = settings.autoBuild;
    cfg::buildDev = settings.buildDev;
    cfg::discordBranch = settings.discordBranch;

    ui::setCommandHandler(handleCommand);
    if (!ui::create(instance)) {
        CoUninitialize();
        return 1;
    }

    std::string saved = cfg::equicordPath;
    if (!saved.empty()) {
        equicord::FolderCheck check = equicord::validate(saved);
        if (!check.isValid) {
            std::string guessed = equicord::guessInstallFolder();
            if (!guessed.empty() && equicord::validate(guessed).isValid)
                cfg::equicordPath = guessed;
        }
    } else {
        cfg::equicordPath = equicord::guessInstallFolder();
    }
    if (!cfg::equicordPath.empty())
        applyEquicordPath(cfg::equicordPath);
    else
        ui::setEquicordPath("", false, "Select your Equicord folder to get started. Browse to the folder with package.json, src/, and scripts/");

    ui::setDiscordBranch(cfg::discordBranch);
    refreshCatalog();
    int code = ui::runMessageLoop();
    CoUninitialize();
    return code;
}
