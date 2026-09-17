#include "plugin.h"
#include "archive.h"
#include "http.h"
#include "json.h"
#include "paths.h"
#include "text.h"
#include <cctype>
#include <cstdio>

static int versionPart(const std::string& version, int index) {
    int partIndex = 0;
    int value = 0;
    bool inPart = false;
    for (char ch : version) {
        if (ch >= '0' && ch <= '9') {
            inPart = true;
            value = value * 10 + (ch - '0');
            continue;
        }
        if (inPart) {
            if (partIndex == index)
                return value;
            partIndex++;
            value = 0;
            inPart = false;
        }
    }
    if (inPart && partIndex == index)
        return value;
    return 0;
}

bool plugins::versionIsNewer(const std::string& remoteVersion, const std::string& localVersion) {
    if (localVersion.empty() && !remoteVersion.empty())
        return true;
    for (int i = 0; i < 3; i++) {
        int remote = versionPart(remoteVersion, i);
        int local = versionPart(localVersion, i);
        if (remote > local)
            return true;
        if (remote < local)
            return false;
    }
    return false;
}

static std::string readInstalledVersion(const std::string& pluginDir) {
    (void)pluginDir;
    return "";
}

static Outcome writeMeta(const std::string& pluginDir, const CatalogPlugin& plugin) {
    (void)pluginDir;
    (void)plugin;
    return ok();
}

std::vector<InstalledPlugin> plugins::scanInstalled(const std::string& equicordRoot) {
    std::vector<InstalledPlugin> installed;
    std::string root = paths::userPluginsFolder(equicordRoot);
    for (const std::string& name : paths::listSubfolders(root)) {
        if (!paths::isSafePluginId(name))
            continue;
        InstalledPlugin plugin;
        plugin.id = name;
        plugin.hasIndex = paths::hasPluginIndex(paths::join(root, name));
        plugin.version = readInstalledVersion(paths::join(root, name));
        installed.push_back(plugin);
    }
    return installed;
}

std::vector<PluginStatus> plugins::mergeStatus(const std::vector<CatalogPlugin>& catalog, const std::string& equicordRoot) {
    std::vector<InstalledPlugin> installed = scanInstalled(equicordRoot);
    std::vector<PluginStatus> statuses;
    for (const CatalogPlugin& item : catalog) {
        PluginStatus status;
        status.catalog = item;
        for (const InstalledPlugin& local : installed) {
            if (local.id != item.id)
                continue;
            status.isInstalled = local.hasIndex;
            status.installedVersion = local.version;
            status.updateAvailable = status.isInstalled && versionIsNewer(item.version, local.version);
            break;
        }
        statuses.push_back(status);
    }
    for (const InstalledPlugin& local : installed) {
        bool listed = false;
        for (const PluginStatus& status : statuses) {
            if (status.catalog.id == local.id) {
                listed = true;
                break;
            }
        }
        if (listed)
            continue;
        PluginStatus extra;
        extra.catalog.id = local.id;
        extra.catalog.name = local.id;
        extra.catalog.description = "Already in src/userplugins";
        extra.catalog.version = local.version.empty() ? "0.0.0" : local.version;
        extra.isInstalled = local.hasIndex;
        extra.installedVersion = local.version;
        statuses.push_back(extra);
    }
    return statuses;
}

static void rewriteGithubUrl(std::string& url, std::string& sourcePath);

static std::string stripQueryAndSlash(std::string text) {
    size_t query = text.find('?');
    if (query != std::string::npos)
        text = text.substr(0, query);
    while (!text.empty() && (text.back() == '/' || text.back() == '\\'))
        text.pop_back();
    return text;
}

static std::string lastPathSegment(std::string text) {
    text = stripQueryAndSlash(text);
    size_t slash = text.find_last_of("/\\");
    std::string name = slash == std::string::npos ? text : text.substr(slash + 1);
    std::string lower = toLowerAscii(name);
    if (lower == "index.ts" || lower == "index.tsx") {
        std::string parent = slash == std::string::npos ? "" : text.substr(0, slash);
        size_t parentSlash = parent.find_last_of("/\\");
        name = parentSlash == std::string::npos ? parent : parent.substr(parentSlash + 1);
        lower = toLowerAscii(name);
    }
    const char* suffixes[] = { ".zip", ".rar", ".7z" };
    for (const char* suffix : suffixes) {
        size_t n = 0;
        while (suffix[n])
            n++;
        if (lower.size() >= n && lower.compare(lower.size() - n, n, suffix) == 0) {
            name = name.substr(0, name.size() - n);
            break;
        }
    }
    return name;
}

static std::string makePluginId(const std::string& raw) {
    std::string id;
    for (unsigned char ch : raw) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
            id.push_back((char)ch);
    }
    if (id.empty())
        id = "userPlugin";
    if (id[0] >= '0' && id[0] <= '9')
        id.insert(id.begin(), 'p');
    if (!paths::isSafePluginId(id))
        id = "userPlugin";
    return id;
}

CatalogPlugin plugins::fromSource(const std::string& source) {
    CatalogPlugin plugin;
    plugin.downloadUrl = source;
    plugin.id = makePluginId(lastPathSegment(source));
    plugin.name = lastPathSegment(source);
    if (plugin.name.empty())
        plugin.name = plugin.id;
    plugin.description = "Unofficial userplugin";
    plugin.version = "0.0.0";
    std::string url = source;
    rewriteGithubUrl(url, plugin.sourcePath);
    return plugin;
}

static std::string fileNameFromUrl(const std::string& url) {
    std::string path = url;
    size_t q = path.find('?');
    if (q != std::string::npos)
        path = path.substr(0, q);
    std::string name = paths::fileName(path);
    if (name.empty())
        return "index.ts";
    return name;
}

static void rewriteGithubUrl(std::string& url, std::string& sourcePath) {
    const char* prefix = "https://github.com/";
    if (!startsWith(toLowerAscii(url), prefix) && !startsWith(toLowerAscii(url), "http://github.com/"))
        return;

    size_t host = url.find("github.com/");
    if (host == std::string::npos)
        return;
    std::string rest = url.substr(host + 11);
    while (!rest.empty() && rest.back() == '/')
        rest.pop_back();

    size_t slash1 = rest.find('/');
    if (slash1 == std::string::npos)
        return;
    size_t slash2 = rest.find('/', slash1 + 1);
    std::string owner = rest.substr(0, slash1);
    std::string repo = (slash2 == std::string::npos) ? rest.substr(slash1 + 1) : rest.substr(slash1 + 1, slash2 - slash1 - 1);
    std::string after = (slash2 == std::string::npos) ? "" : rest.substr(slash2 + 1);

    if (startsWith(after, "archive/"))
        return;

    if (startsWith(after, "blob/")) {
        std::string blob = after.substr(5);
        size_t branchEnd = blob.find('/');
        if (branchEnd == std::string::npos)
            return;
        std::string branch = blob.substr(0, branchEnd);
        std::string file = blob.substr(branchEnd + 1);
        if (paths::isPluginSourceFile(file)) {
            size_t slash = file.find_last_of('/');
            std::string nested = slash == std::string::npos ? "" : file.substr(0, slash);
            url = "https://github.com/" + owner + "/" + repo + "/archive/refs/heads/" + branch + ".zip";
            if (!nested.empty() && sourcePath.empty())
                sourcePath = nested;
            return;
        }
        url = "https://raw.githubusercontent.com/" + owner + "/" + repo + "/" + branch + "/" + file;
        return;
    }

    std::string branch = "main";
    std::string nested;
    if (startsWith(after, "tree/")) {
        std::string tree = after.substr(5);
        size_t branchEnd = tree.find('/');
        if (branchEnd == std::string::npos) {
            branch = tree;
        } else {
            branch = tree.substr(0, branchEnd);
            nested = tree.substr(branchEnd + 1);
        }
    } else if (!after.empty()) {
        return;
    }

    bool isCommit = branch.size() >= 7 && branch.size() <= 40;
    if (isCommit) {
        for (char c : branch) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                isCommit = false;
                break;
            }
        }
    }
    if (isCommit) {
        url = "https://github.com/" + owner + "/" + repo + "/archive/" + branch + ".zip";
    } else {
        url = "https://github.com/" + owner + "/" + repo + "/archive/refs/heads/" + branch + ".zip";
    }
    if (!nested.empty() && sourcePath.empty())
        sourcePath = nested;
}

struct GithubFolder {
    std::string owner;
    std::string repo;
    std::string ref;
    std::string path;
    bool valid = false;
};

static GithubFolder parseGithubFolder(const std::string& rawUrl) {
    GithubFolder folder;
    std::string lower = toLowerAscii(rawUrl);
    if (!startsWith(lower, "https://github.com/") && !startsWith(lower, "http://github.com/"))
        return folder;

    size_t host = rawUrl.find("github.com/");
    if (host == std::string::npos)
        return folder;
    std::string rest = stripQueryAndSlash(rawUrl.substr(host + 11));

    size_t slash1 = rest.find('/');
    if (slash1 == std::string::npos)
        return folder;
    size_t slash2 = rest.find('/', slash1 + 1);
    if (slash2 == std::string::npos)
        return folder;

    folder.owner = rest.substr(0, slash1);
    folder.repo = rest.substr(slash1 + 1, slash2 - slash1 - 1);

    std::string after = rest.substr(slash2 + 1);
    if (!startsWith(after, "tree/"))
        return folder;

    std::string tree = after.substr(5);
    size_t branchEnd = tree.find('/');
    if (branchEnd == std::string::npos)
        return folder;

    folder.ref = tree.substr(0, branchEnd);
    folder.path = tree.substr(branchEnd + 1);
    folder.valid = !folder.owner.empty() && !folder.repo.empty()
        && !folder.ref.empty() && !folder.path.empty();
    return folder;
}

static Outcome fetchGithubFolder(
    const GithubFolder& folder,
    const std::string& destFolder,
    const std::string& relativePrefix,
    const std::function<void(const std::string&)>& progress,
    int depth
) {
    if (depth > 3)
        return ok();

    std::string api = "https://api.github.com/repos/" + folder.owner + "/" + folder.repo
        + "/contents/" + folder.path + "?ref=" + folder.ref;

    http::Response listing = http::get(api, 4 * 1024 * 1024);
    if (!listing.succeeded)
        return fail(listing.userMessage, api + " -> " + listing.detail);
    if (listing.body.empty() || listing.body[0] != '[')
        return fail("GitHub didn't return a folder listing for that link.", api);

    bool copiedAny = false;
    for (const std::string& item : json::parseObjectArray(listing.body)) {
        std::string name = json::getString(item, "name");
        std::string type = json::getString(item, "type");
        if (name.empty() || !paths::isSafeRelativeFile(name))
            continue;

        std::string relative = relativePrefix.empty() ? name : relativePrefix + "/" + name;

        if (type == "dir") {
            GithubFolder child = folder;
            child.path = folder.path + "/" + name;
            Outcome nested = fetchGithubFolder(child, destFolder, relative, progress, depth + 1);
            if (!nested.succeeded)
                return nested;
            continue;
        }

        if (type != "file" || !paths::isPluginSourceFile(name))
            continue;

        std::string downloadUrl = json::getString(item, "download_url");
        if (downloadUrl.empty())
            continue;
        if (progress)
            progress("Downloading " + relative);

        http::Response file = http::get(downloadUrl, 8 * 1024 * 1024);
        if (!file.succeeded)
            return fail(file.userMessage, downloadUrl + " -> " + file.detail);

        Outcome written = paths::writeFileIfSafe(paths::join(destFolder, relative), file.body, destFolder);
        if (!written.succeeded)
            return written;
        copiedAny = true;
    }

    if (!copiedAny && depth == 0)
        return fail("That GitHub folder had no .ts, .tsx or .css files in it.", api);
    return ok();
}

static Outcome copyLocalSource(const std::string& source, const std::string& staging, const std::string& sourcePath) {
    if (paths::folderExists(source)) {
        std::string from = source;
        if (!sourcePath.empty())
            from = paths::join(source, sourcePath);
        return paths::copyTreeIfSafe(from, staging, staging);
    }
    if (paths::fileExists(source)) {
        if (archive::isArchivePath(source)) {
            std::string extracted = staging + ".__extract";
            Outcome unpacked = archive::extractArchive(source, extracted);
            if (!unpacked.succeeded)
                return unpacked;
            std::string found = archive::locatePluginSource(extracted, sourcePath);
            if (found.empty() || !paths::folderExists(found)) {
                paths::deleteFolderIfSafe(extracted, paths::parentFolder(extracted));
                return fail("The archive didn't contain a plugin folder with index.ts or index.tsx.");
            }
            Outcome copied = paths::copyTreeIfSafe(found, staging, staging);
            paths::deleteFolderIfSafe(extracted, paths::parentFolder(extracted));
            return copied;
        }
        if (paths::isPluginSourceFile(source))
            return paths::copyTreeIfSafe(paths::parentFolder(source), staging, staging);
        std::string name = paths::fileName(source);
        return paths::copyFileIfSafe(source, paths::join(staging, name), staging);
    }
    return fail(
        "The plugin files weren't found at that path. Use a GitHub URL or a local folder/zip path.",
        source
    );
}

static Outcome fetchRemotePlugin(const CatalogPlugin& plugin, const std::string& staging, const std::function<void(const std::string&)>& progress) {
    std::string url = plugin.downloadUrl;
    std::string sourcePath = plugin.sourcePath;
    rewriteGithubUrl(url, sourcePath);
    if (progress)
        progress("Downloading " + plugin.name);

    GithubFolder folder = parseGithubFolder(plugin.downloadUrl);
    if (folder.valid) {
        Outcome direct = fetchGithubFolder(folder, staging, "", progress, 0);
        if (direct.succeeded && paths::hasPluginIndex(staging))
            return direct;
        if (progress)
            progress("Falling back to the full repository download...");
        paths::deleteFolderIfSafe(staging, staging);
        paths::ensureFolder(staging);
    }

    std::string tempRoot = paths::join(paths::ensureTempFolder(), plugin.id + "-dl");
    paths::ensureFolder(tempRoot);
    std::string blobPath = paths::join(tempRoot, "download.bin");

    http::Response response = http::get(url, 80 * 1024 * 1024);
    if (!response.succeeded) {
        paths::deleteFolderIfSafe(tempRoot, paths::ensureTempFolder());
        if (response.detail.find("404") != std::string::npos || response.detail.find("Not Found") != std::string::npos) {
            return fail("The download failed . server returned 404. Check the GitHub link, especially the branch or commit hash.", url + " → " + response.detail);
        }
        return fail(response.userMessage, response.detail);
    }
    if (response.body.size() > 0 && response.body[0] == '<') {
        std::string snippet = response.body.substr(0, 800);
        if (snippet.find("404") != std::string::npos || snippet.find("Not Found") != std::string::npos) {
            paths::deleteFolderIfSafe(tempRoot, paths::ensureTempFolder());
            return fail("The download failed. server returned 404. Check the GitHub link, especially the branch or commit hash.", url);
        }
    }

    Outcome saved = paths::writeFileIfSafe(blobPath, response.body, tempRoot);
    if (!saved.succeeded) {
        paths::deleteFolderIfSafe(tempRoot, paths::ensureTempFolder());
        return saved;
    }

    Outcome copied;
    if (archive::isArchivePath(url) || archive::isArchiveBytes(response.body)) {
        std::string extracted = paths::join(tempRoot, "extracted");
        copied = archive::extractArchive(blobPath, extracted);
        if (copied.succeeded) {
            std::string found = archive::locatePluginSource(extracted, sourcePath);
            if (found.empty() || !paths::hasPluginIndex(found)) {
                copied = fail(
                    "The download extracted, but no index.ts or index.tsx was in that folder.",
                    found
                );
            } else {
                copied = paths::copyTreeIfSafe(found, staging, staging);
            }
        }
    } else {
        std::string name = fileNameFromUrl(url);
        if (!paths::isSafeRelativeFile(name))
            name = "index.ts";
        copied = paths::copyFileIfSafe(blobPath, paths::join(staging, name), staging);
    }

    paths::deleteFolderIfSafe(tempRoot, paths::ensureTempFolder());
    return copied;
}

static Outcome fetchListedFiles(const CatalogPlugin& plugin, const std::string& staging, const std::function<void(const std::string&)>& progress) {
    for (size_t i = 0; i < plugin.files.size(); i++) {
        const PluginFile& file = plugin.files[i];
        if (!paths::isSafeRelativeFile(file.relativePath)) {
            return fail(
                "A file path was unsafe, so the install was cancelled.",
                file.relativePath
            );
        }
        if (progress)
            progress("Downloading " + file.relativePath);

        std::string dest = paths::join(staging, file.relativePath);
        if (http::isRemoteUrl(file.url)) {
            http::Response response = http::get(file.url, 8 * 1024 * 1024);
            if (!response.succeeded)
                return fail(response.userMessage, response.detail);
            Outcome written = paths::writeFileIfSafe(dest, response.body, staging);
            if (!written.succeeded)
                return written;
        } else {
            Outcome copied = paths::copyFileIfSafe(file.url, dest, staging);
            if (!copied.succeeded)
                return copied;
        }
    }
    return ok();
}

Outcome plugins::install(
    const CatalogPlugin& plugin,
    const std::string& equicordRoot,
    const std::function<void(const std::string& status)>& progress
) {
    if (!paths::isSafePluginId(plugin.id)) {
        return fail(
            "That plugin id isn't allowed. Use letters, numbers, hyphen, or underscore only, starting with a letter."
        );
    }
    if (plugin.files.empty() && plugin.downloadUrl.empty()) {
        return fail(
            "The plugin couldn't install. no download URL or files were provided. Use a GitHub link or local folder."
        );
    }

    std::string userPlugins = paths::userPluginsFolder(equicordRoot);
    Outcome created = paths::ensureFolder(userPlugins);
    if (!created.succeeded)
        return created;


    paths::cleanWorkFolders(userPlugins);


    std::string staging = paths::join(userPlugins, "_" + plugin.id + ".__installing");
    paths::deleteFolderIfSafe(staging, userPlugins);
    created = paths::ensureFolder(staging);
    if (!created.succeeded)
        return created;

    Outcome fetched;
    if (!plugin.files.empty())
        fetched = fetchListedFiles(plugin, staging, progress);
    else if (http::isRemoteUrl(plugin.downloadUrl))
        fetched = fetchRemotePlugin(plugin, staging, progress);
    else
        fetched = copyLocalSource(plugin.downloadUrl, staging, plugin.sourcePath);

    if (!fetched.succeeded) {
        paths::deleteFolderIfSafe(staging, userPlugins);
        return fetched;
    }

    if (!paths::hasPluginIndex(staging)) {
        paths::deleteFolderIfSafe(staging, userPlugins);
        return fail(
            "The plugin couldn't install because the download had no index.ts or index.tsx. Equicord needs that file in the plugin folder."
        );
    }

    {
        std::vector<std::string> equicordPlugins;
        std::string equicordSrcPlugins = paths::join(paths::join(equicordRoot, "src"), "equicordplugins");
        for (auto& name : paths::listSubfolders(equicordSrcPlugins)) {
            equicordPlugins.push_back(name);
        }
        std::vector<std::string> toPatch;
        paths::walkFiles(staging, [&](const std::string& rel, const std::string& full){
            if (paths::isPluginSourceFile(rel)) toPatch.push_back(full);
        });
        for (auto& filePath : toPatch) {
            std::string content = paths::readFile(filePath);
            std::string patched = content;
            for (auto& dep : equicordPlugins) {
                std::string needle1 = "\"../" + dep + "\"";
                std::string repl1 = "\"@equicordplugins/" + dep + "\"";
                size_t pos = 0;
                while ((pos = patched.find(needle1, pos)) != std::string::npos) {
                    patched.replace(pos, needle1.size(), repl1);
                    pos += repl1.size();
                }
                std::string needle2 = "'../" + dep + "'";
                std::string repl2 = "'@equicordplugins/" + dep + "'";
                pos = 0;
                while ((pos = patched.find(needle2, pos)) != std::string::npos) {
                    patched.replace(pos, needle2.size(), repl2);
                    pos += repl2.size();
                }

                std::string needle3 = "\"../" + dep + "/";
                std::string repl3 = "\"@equicordplugins/" + dep + "/";
                pos = 0;
                while ((pos = patched.find(needle3, pos)) != std::string::npos) {
                    patched.replace(pos, needle3.size(), repl3);
                    pos += repl3.size();
                }
                std::string needle4 = "'../" + dep + "/";
                std::string repl4 = "'@equicordplugins/" + dep + "/";
                pos = 0;
                while ((pos = patched.find(needle4, pos)) != std::string::npos) {
                    patched.replace(pos, needle4.size(), repl4);
                    pos += repl4.size();
                }
            }
            if (patched != content) {
                paths::writeFileIfSafe(filePath, patched, staging);
            }
        }
    }

    writeMeta(staging, plugin);
    if (progress)
        progress("Installing " + plugin.name);

    std::string finalFolder = paths::pluginFolder(equicordRoot, plugin.id);
    Outcome swapped = paths::replaceFolderIfSafe(staging, finalFolder, userPlugins);
    if (!swapped.succeeded) {
        paths::deleteFolderIfSafe(staging, userPlugins);
        return swapped;
    }
    return ok(plugin.name + " is installed to src/userplugins/" + plugin.id + ".");
}

Outcome plugins::removePlugin(const std::string& pluginId, const std::string& equicordRoot) {
    if (!paths::isSafePluginId(pluginId))
        return fail("That plugin id isn't allowed, so nothing was deleted.");
    std::string userPlugins = paths::userPluginsFolder(equicordRoot);
    std::string folder = paths::pluginFolder(equicordRoot, pluginId);
    if (!paths::folderExists(folder))
        return ok("That plugin is not installed.");
    Outcome removed = paths::deleteFolderIfSafe(folder, userPlugins);
    if (!removed.succeeded)
        return removed;
    return ok(pluginId + " was removed from src/userplugins.");
}
