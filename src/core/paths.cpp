#include "paths.h"
#include "text.h"
#include "win32.h"
#include <fstream>
#include <shlobj.h>

struct FindHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit FindHandle(HANDLE value) : handle(value) {}
    ~FindHandle() {
        if (handle != INVALID_HANDLE_VALUE)
            FindClose(handle);
    }
    FindHandle(const FindHandle&) = delete;
    FindHandle& operator=(const FindHandle&) = delete;
    bool valid() const { return handle != INVALID_HANDLE_VALUE; }
};

static bool isDotOrJunction(const WIN32_FIND_DATAW& data) {
    if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
        return true;
    return (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

static std::string slashToBack(std::string path) {
    for (char& ch : path) {
        if (ch == '/')
            ch = '\\';
    }
    return path;
}

bool paths::isSafePluginId(const std::string& pluginId) {
    if (pluginId.empty() || pluginId.size() > 64)
        return false;
    unsigned char first = (unsigned char)pluginId[0];
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')))
        return false;
    for (unsigned char ch : pluginId) {
        bool ok = (ch >= 'A' && ch <= 'Z')
            || (ch >= 'a' && ch <= 'z')
            || (ch >= '0' && ch <= '9')
            || ch == '_' || ch == '-';
        if (!ok)
            return false;
    }
    return true;
}

bool paths::isSafeRelativeFile(const std::string& relativePath) {
    if (relativePath.empty() || relativePath.size() > 240)
        return false;
    if (relativePath[0] == '/' || relativePath[0] == '\\')
        return false;
    if (relativePath.find(':') != std::string::npos)
        return false;
    if (relativePath.find("..") != std::string::npos)
        return false;
    for (unsigned char ch : relativePath) {
        if (ch < 32)
            return false;
    }
    return true;
}

bool paths::isPluginSourceFile(const std::string& path) {
    std::string name = toLowerAscii(fileName(path));
    if (name.size() >= 4 && name.substr(name.size() - 4) == ".tsx")
        return true;
    if (name.size() >= 4 && name.substr(name.size() - 4) == ".css")
        return true;
    return name.size() >= 3 && name.substr(name.size() - 3) == ".ts";
}

bool paths::isSafeArchiveEntry(const std::string& entry) {
    if (entry.empty())
        return false;
    if (entry[0] == '/' || entry[0] == '\\')
        return false;
    if (entry.find(':') != std::string::npos)
        return false;

    std::string normalized = entry;
    for (char& ch : normalized) {
        if (ch == '/')
            ch = '\\';
    }

    size_t start = 0;
    while (start <= normalized.size()) {
        size_t end = normalized.find('\\', start);
        if (end == std::string::npos)
            end = normalized.size();
        std::string segment = normalized.substr(start, end - start);
        if (segment == "..")
            return false;
        start = end + 1;
        if (end == normalized.size())
            break;
    }
    return true;
}

std::string paths::join(const std::string& left, const std::string& right) {
    if (left.empty())
        return slashToBack(right);
    if (right.empty())
        return slashToBack(left);
    std::string a = slashToBack(left);
    std::string b = slashToBack(right);
    while (!a.empty() && (a.back() == '\\' || a.back() == '/'))
        a.pop_back();
    while (!b.empty() && (b.front() == '\\' || b.front() == '/'))
        b.erase(b.begin());
    return a + "\\" + b;
}

std::string paths::canonicalize(const std::string& path) {
    std::wstring input = wideFromUtf8(slashToBack(path));
    if (input.empty())
        return "";
    wchar_t buffer[MAX_PATH * 4];
    DWORD n = GetFullPathNameW(input.c_str(), MAX_PATH * 4, buffer, nullptr);
    if (n == 0 || n >= MAX_PATH * 4)
        return slashToBack(path);
    return utf8FromWide(buffer);
}

bool paths::isInside(const std::string& child, const std::string& parent) {
    std::string childPath = toLowerAscii(canonicalize(child));
    std::string parentPath = toLowerAscii(canonicalize(parent));
    if (childPath.empty() || parentPath.empty())
        return false;
    while (!parentPath.empty() && parentPath.back() == '\\')
        parentPath.pop_back();
    if (childPath == parentPath)
        return true;
    std::string prefix = parentPath + "\\";
    return childPath.size() >= prefix.size() && childPath.compare(0, prefix.size(), prefix) == 0;
}

bool paths::fileExists(const std::string& path) {
    DWORD attr = GetFileAttributesW(wideFromUtf8(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool paths::folderExists(const std::string& path) {
    DWORD attr = GetFileAttributesW(wideFromUtf8(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

Outcome paths::ensureFolder(const std::string& path) {
    std::string full = canonicalize(path);
    if (full.empty())
        return fail("Couldn't create the folder because the path was empty.");
    int result = SHCreateDirectoryExW(nullptr, wideFromUtf8(full).c_str(), nullptr);
    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS || result == ERROR_FILE_EXISTS)
        return ok();
    return fail(
        "Couldn't create the folder " + full + ". Check that you can write there, then try again.",
        "SHCreateDirectoryExW " + std::to_string(result)
    );
}

static Outcome removeTree(const std::wstring& folder) {
    WIN32_FIND_DATAW data;
    FindHandle find(FindFirstFileW((folder + L"\\*").c_str(), &data));
    if (!find.valid())
        return ok();

    do {
        if (isDotOrJunction(data))
            continue;

        std::wstring child = folder + L"\\" + data.cFileName;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            Outcome nested = removeTree(child);
            if (!nested.succeeded)
                return nested;
            if (!RemoveDirectoryW(child.c_str()))
                return fail("Couldn't delete a plugin folder. Close Discord if it has those files open, then try again.");
        } else {
            SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFileW(child.c_str()))
                return fail("Couldn't delete a plugin file. Close Discord if it has those files open, then try again.");
        }
    } while (FindNextFileW(find.handle, &data));

    return ok();
}

Outcome paths::deleteFolderIfSafe(const std::string& folder, const std::string& allowedRoot) {
    if (!folderExists(folder))
        return ok();
    if (!isInside(folder, allowedRoot)) {
        return fail(
            "The app refused to delete that folder because it is outside the selected Equicord userplugins directory.",
            canonicalize(folder) + " not inside " + canonicalize(allowedRoot)
        );
    }
    Outcome cleared = removeTree(wideFromUtf8(canonicalize(folder)));
    if (!cleared.succeeded)
        return cleared;
    RemoveDirectoryW(wideFromUtf8(canonicalize(folder)).c_str());
    return ok();
}

Outcome paths::copyFileIfSafe(const std::string& source, const std::string& dest, const std::string& allowedRoot) {
    if (!isInside(dest, allowedRoot)) {
        return fail(
            "The plugin archive tried to write outside its own folder, so the install was cancelled.",
            dest
        );
    }
    std::string destFolder = parentFolder(dest);
    Outcome created = ensureFolder(destFolder);
    if (!created.succeeded)
        return created;
    if (!CopyFileW(wideFromUtf8(source).c_str(), wideFromUtf8(dest).c_str(), FALSE)) {
        return fail(
            "Couldn't copy a plugin file into Equicord. Check disk space and folder permissions, then try again.",
            "CopyFileW " + std::to_string(GetLastError())
        );
    }
    return ok();
}

Outcome paths::copyTreeIfSafe(const std::string& sourceFolder, const std::string& destFolder, const std::string& allowedRoot) {
    if (!folderExists(sourceFolder))
        return fail("The plugin files were missing after download. Refresh the catalog and try again.");
    if (!isInside(destFolder, allowedRoot))
        return fail("The app refused to copy files outside the selected Equicord userplugins directory.");

    Outcome created = ensureFolder(destFolder);
    if (!created.succeeded)
        return created;

    Outcome result = ok();
    walkFiles(sourceFolder, [&](const std::string& relative, const std::string& full) {
        if (!result.succeeded)
            return;
        if (!isPluginSourceFile(relative))
            return;
        if (!isSafeRelativeFile(relative)) {
            result = fail(
                "The plugin archive contained an unsafe path, so the install was cancelled.",
                relative
            );
            return;
        }
        result = copyFileIfSafe(full, join(destFolder, relative), allowedRoot);
    });
    return result;
}

Outcome paths::replaceFolderIfSafe(const std::string& stagingFolder, const std::string& finalFolder, const std::string& allowedRoot) {
    if (!isInside(stagingFolder, allowedRoot) || !isInside(finalFolder, allowedRoot))
        return fail("The app refused to replace a folder outside userplugins.");

    std::string backup = join(parentFolder(finalFolder), "_" + fileName(finalFolder) + ".__backup");
    deleteFolderIfSafe(backup, allowedRoot);

    if (folderExists(finalFolder)) {
        if (!MoveFileW(wideFromUtf8(finalFolder).c_str(), wideFromUtf8(backup).c_str())) {
            return fail(
                "Couldn't replace the existing plugin. Close Discord if it has those files open, then try again.",
                "MoveFile backup " + std::to_string(GetLastError())
            );
        }
    }

    if (!MoveFileW(wideFromUtf8(stagingFolder).c_str(), wideFromUtf8(finalFolder).c_str())) {
        if (folderExists(backup))
            MoveFileW(wideFromUtf8(backup).c_str(), wideFromUtf8(finalFolder).c_str());
        return fail(
            "Couldn't finish installing the plugin. The previous copy was left in place.",
            "MoveFile final " + std::to_string(GetLastError())
        );
    }

    deleteFolderIfSafe(backup, allowedRoot);
    return ok();
}

std::string paths::parentFolder(const std::string& path) {
    std::string full = slashToBack(path);
    size_t pos = full.find_last_of('\\');
    if (pos == std::string::npos)
        return "";
    return full.substr(0, pos);
}

std::string paths::fileName(const std::string& path) {
    std::string full = slashToBack(path);
    size_t pos = full.find_last_of('\\');
    if (pos == std::string::npos)
        return full;
    return full.substr(pos + 1);
}

std::string paths::exeFolder() {
    wchar_t buffer[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buffer, MAX_PATH * 4);
    if (n == 0)
        return "";
    return parentFolder(utf8FromWide(buffer));
}

std::string paths::ensureTempFolder() {
    wchar_t buffer[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, buffer);
    if (n == 0)
        return exeFolder();
    std::string root = join(utf8FromWide(buffer), "EquicordPluginManager");
    ensureFolder(root);
    return root;
}

std::string paths::userPluginsFolder(const std::string& equicordRoot) {
    return join(join(equicordRoot, "src"), "userplugins");
}

std::string paths::pluginFolder(const std::string& equicordRoot, const std::string& pluginId) {
    return join(userPluginsFolder(equicordRoot), pluginId);
}

std::string paths::readFile(const std::string& path) {
    std::ifstream file(wideFromUtf8(path).c_str(), std::ios::binary);
    if (!file)
        return "";
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

Outcome paths::writeFileIfSafe(const std::string& path, const std::string& contents, const std::string& allowedRoot) {
    if (!isInside(path, allowedRoot))
        return fail("The app refused to write a file outside the selected Equicord folder.");
    Outcome created = ensureFolder(parentFolder(path));
    if (!created.succeeded)
        return created;
    std::ofstream file(wideFromUtf8(path).c_str(), std::ios::binary);
    if (!file)
        return fail("Couldn't write a plugin file. Check folder permissions, then try again.");
    file << contents;
    return ok();
}

std::vector<std::string> paths::listSubfolders(const std::string& folder) {
    std::vector<std::string> names;
    if (!folderExists(folder))
        return names;

    WIN32_FIND_DATAW data;
    FindHandle find(FindFirstFileW(wideFromUtf8(join(folder, "*")).c_str(), &data));
    if (!find.valid())
        return names;

    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        if (isDotOrJunction(data))
            continue;
        std::string name = utf8FromWide(data.cFileName);
        if (name.find(".__") != std::string::npos)
            continue;
        names.push_back(name);
    } while (FindNextFileW(find.handle, &data));

    return names;
}

std::vector<paths::DirEntry> paths::listEntries(const std::string& folder) {
    std::vector<DirEntry> entries;
    if (!folderExists(folder))
        return entries;

    WIN32_FIND_DATAW data;
    FindHandle find(FindFirstFileW(wideFromUtf8(join(folder, "*")).c_str(), &data));
    if (!find.valid())
        return entries;

    do {
        if (isDotOrJunction(data))
            continue;
        DirEntry entry;
        entry.name = utf8FromWide(data.cFileName);
        entry.isFolder = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entries.push_back(entry);
    } while (FindNextFileW(find.handle, &data));

    return entries;
}

Outcome paths::cleanWorkFolders(const std::string& userPluginsRoot) {
    if (!folderExists(userPluginsRoot))
        return ok();
    for (const DirEntry& entry : listEntries(userPluginsRoot)) {
        if (!entry.isFolder)
            continue;
        if (entry.name.find(".__") == std::string::npos)
            continue;
        deleteFolderIfSafe(join(userPluginsRoot, entry.name), userPluginsRoot);
    }
    return ok();
}

bool paths::hasPluginIndex(const std::string& folder) {
    return fileExists(join(folder, "index.ts")) || fileExists(join(folder, "index.tsx"));
}

void paths::walkFiles(const std::string& folder, const std::function<void(const std::string& relative, const std::string& full)>& visit) {
    std::function<void(const std::string&, const std::string&)> walk = [&](const std::string& current, const std::string& prefix) {
        WIN32_FIND_DATAW data;
        FindHandle find(FindFirstFileW(wideFromUtf8(join(current, "*")).c_str(), &data));
        if (!find.valid())
            return;
        do {
            if (isDotOrJunction(data))
                continue;
            std::string name = utf8FromWide(data.cFileName);
            std::string relative = prefix.empty() ? name : prefix + "\\" + name;
            std::string full = join(current, name);
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                walk(full, relative);
            else
                visit(relative, full);
        } while (FindNextFileW(find.handle, &data));
    };
    walk(folder, "");
}
