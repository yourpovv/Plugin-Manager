#include "manifest.h"
#include "http.h"
#include "json.h"
#include "log.h"
#include "paths.h"
#include "text.h"

static std::string resolveLocalPath(const std::string& urlOrPath) {
    if (paths::fileExists(urlOrPath) || paths::folderExists(urlOrPath))
        return paths::canonicalize(urlOrPath);
    std::string nextToExe = paths::join(paths::exeFolder(), urlOrPath);
    if (paths::fileExists(nextToExe) || paths::folderExists(nextToExe))
        return nextToExe;
    return urlOrPath;
}

static CatalogPlugin parsePlugin(const std::string& object) {
    CatalogPlugin plugin;
    plugin.id = json::getString(object, "id");
    plugin.name = json::getString(object, "name");
    plugin.description = json::getString(object, "description");
    plugin.author = json::getString(object, "author");
    plugin.version = json::getString(object, "version");
    plugin.downloadUrl = json::getString(object, "download");
    plugin.sourcePath = json::getString(object, "sourcePath");

    if (plugin.name.empty())
        plugin.name = plugin.id;
    if (plugin.version.empty())
        plugin.version = "0.0.0";

    for (const std::string& fileObject : json::getObjectArray(object, "files")) {
        PluginFile file;
        file.url = json::getString(fileObject, "url");
        file.relativePath = json::getString(fileObject, "path");
        if (file.url.empty() || file.relativePath.empty())
            continue;
        plugin.files.push_back(file);
    }
    return plugin;
}

static void resolveRelativeDownloads(CatalogPlugin& plugin, const std::string& catalogFolder) {
    if (catalogFolder.empty())
        return;
    if (!plugin.downloadUrl.empty() && !http::isRemoteUrl(plugin.downloadUrl)) {
        std::string joined = paths::join(catalogFolder, plugin.downloadUrl);
        if (paths::fileExists(joined) || paths::folderExists(joined))
            plugin.downloadUrl = joined;
    }
    for (PluginFile& file : plugin.files) {
        if (!http::isRemoteUrl(file.url)) {
            std::string joined = paths::join(catalogFolder, file.url);
            if (paths::fileExists(joined))
                file.url = joined;
        }
    }
}

manifest::Catalog manifest::parse(const std::string& jsonText, const std::string& sourceLabel) {
    Catalog catalog;
    catalog.source = sourceLabel;
    if (jsonText.empty()) {
        catalog.userMessage = "The plugin list was empty. Check plugins.json, then click Refresh.";
        return catalog;
    }

    catalog.version = json::getInt(jsonText, "version", 1);
    std::vector<std::string> objects = json::getObjectArray(jsonText, "plugins");
    for (const std::string& object : objects) {
        CatalogPlugin plugin = parsePlugin(object);
        if (!paths::isSafePluginId(plugin.id)) {
            logLine("WARN", "Skipped a catalog entry with an unsafe or missing id.");
            continue;
        }
        catalog.plugins.push_back(plugin);
    }

    catalog.loaded = true;
    if (catalog.plugins.empty()) {
        catalog.userMessage = "The catalog loaded, but it has no usable plugins. Add entries to plugins.json.";
    }
    return catalog;
}

manifest::Catalog manifest::loadFromUrlOrFile(const std::string& urlOrPath) {
    if (urlOrPath.empty()) {
        Catalog catalog;
        catalog.userMessage = "No plugin list URL is set. Open Settings, paste your plugins.json URL, then click Refresh.";
        return catalog;
    }

    if (http::isRemoteUrl(urlOrPath)) {
        http::Response response = http::get(urlOrPath, 2 * 1024 * 1024);
        if (!response.succeeded) {
            Catalog catalog;
            catalog.source = urlOrPath;
            catalog.userMessage = response.userMessage;
            catalog.detail = response.detail;
            return catalog;
        }
        return parse(response.body, urlOrPath);
    }

    std::string path = resolveLocalPath(urlOrPath);
    if (!paths::fileExists(path)) {
        Catalog catalog;
        catalog.source = urlOrPath;
        catalog.userMessage = "The plugin list file wasn't found. Put plugins.json next to the exe, or set a URL in Settings.";
        catalog.detail = path;
        return catalog;
    }

    Catalog catalog = parse(paths::readFile(path), path);
    std::string folder = paths::parentFolder(path);
    for (CatalogPlugin& plugin : catalog.plugins)
        resolveRelativeDownloads(plugin, folder);
    return catalog;
}
