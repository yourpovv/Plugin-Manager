#include "archive.h"
#include "paths.h"
#include "runner.h"
#include "text.h"
#include "win32.h"
#include <vector>

static std::string withoutQuery(std::string text) {
    size_t query = text.find('?');
    if (query != std::string::npos)
        text = text.substr(0, query);
    while (!text.empty() && (text.back() == '/' || text.back() == '\\'))
        text.pop_back();
    return toLowerAscii(text);
}

bool archive::isArchivePath(const std::string& pathOrUrl) {
    std::string name = withoutQuery(pathOrUrl);
    const char* suffixes[] = { ".zip", ".rar", ".7z" };
    for (const char* suffix : suffixes) {
        size_t n = 0;
        while (suffix[n])
            n++;
        if (name.size() >= n && name.compare(name.size() - n, n, suffix) == 0)
            return true;
    }
    return false;
}

bool archive::isArchiveBytes(const std::string& body) {
    if (body.size() >= 4 && body[0] == 'P' && body[1] == 'K')
        return true;
    if (body.size() >= 7 && body.compare(0, 4, "Rar!") == 0)
        return true;
    if (body.size() >= 6 && (unsigned char)body[0] == 0x37 && (unsigned char)body[1] == 0x7A
        && (unsigned char)body[2] == 0xBC && (unsigned char)body[3] == 0xAF)
        return true;
    return false;
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

static std::string sevenZipPath() {
    wchar_t programFiles[MAX_PATH];
    wchar_t programFilesX86[MAX_PATH];
    GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
    GetEnvironmentVariableW(L"ProgramFiles(x86)", programFilesX86, MAX_PATH);
    return firstExisting({
        searchPath(L"7z.exe"),
        paths::join(utf8FromWide(programFiles), "7-Zip\\7z.exe"),
        paths::join(utf8FromWide(programFilesX86), "7-Zip\\7z.exe")
    });
}

static std::string rarToolPath() {
    wchar_t programFiles[MAX_PATH];
    wchar_t programFilesX86[MAX_PATH];
    GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
    GetEnvironmentVariableW(L"ProgramFiles(x86)", programFilesX86, MAX_PATH);
    return firstExisting({
        searchPath(L"UnRAR.exe"),
        searchPath(L"unrar.exe"),
        searchPath(L"WinRAR.exe"),
        paths::join(utf8FromWide(programFiles), "WinRAR\\UnRAR.exe"),
        paths::join(utf8FromWide(programFiles), "WinRAR\\WinRAR.exe"),
        paths::join(utf8FromWide(programFilesX86), "WinRAR\\UnRAR.exe"),
        paths::join(utf8FromWide(programFilesX86), "WinRAR\\WinRAR.exe")
    });
}

static Outcome validateListedEntries(const std::string& listing) {
    size_t start = 0;
    while (start < listing.size()) {
        size_t end = listing.find('\n', start);
        if (end == std::string::npos)
            end = listing.size();
        std::string entry = listing.substr(start, end - start);
        if (!entry.empty() && entry.back() == '\r')
            entry.pop_back();
        start = end + 1;
        if (entry.empty() || entry.back() == '/' || entry.back() == '\\')
            continue;
        if (!paths::isSafeArchiveEntry(entry)) {
            return fail(
                "The plugin archive contained a path that would write outside its folder, so it was not extracted.",
                entry
            );
        }
    }
    return ok();
}

static Outcome extractWithTar(const std::string& archivePath, const std::string& destFolder) {
    std::wstring quoted = L"\"" + wideFromUtf8(archivePath) + L"\"";
    runner::RunResult listed = runner::runCaptured(L"tar.exe -tf " + quoted, paths::parentFolder(archivePath), nullptr);
    if (!listed.started || listed.exitCode != 0)
        return fail("tar could not list the archive.", listed.output);
    Outcome safe = validateListedEntries(listed.output);
    if (!safe.succeeded)
        return safe;
    std::wstring command = L"tar.exe -xf " + quoted + L" -C \"" + wideFromUtf8(destFolder) + L"\"";
    runner::RunResult extracted = runner::runCaptured(command, destFolder, nullptr);
    if (!extracted.started || extracted.exitCode != 0)
        return fail("tar could not extract the archive.", extracted.output);
    return ok();
}

static Outcome extractWithSevenZip(const std::string& archivePath, const std::string& destFolder) {
    std::string sevenZip = sevenZipPath();
    if (sevenZip.empty())
        return fail("7-Zip is not installed.");
    std::wstring command = L"\"" + wideFromUtf8(sevenZip) + L"\" x -y \"-o"
        + wideFromUtf8(destFolder) + L"\" -- \"" + wideFromUtf8(archivePath) + L"\"";
    runner::RunResult extracted = runner::runCaptured(command, destFolder, nullptr);
    if (!extracted.started || extracted.exitCode != 0)
        return fail("7-Zip could not extract the archive.", extracted.output);
    return ok();
}

static Outcome extractWithRar(const std::string& archivePath, const std::string& destFolder) {
    std::string rar = rarToolPath();
    if (rar.empty())
        return fail("WinRAR is not installed.");
    std::wstring command = L"\"" + wideFromUtf8(rar) + L"\" x -y \""
        + wideFromUtf8(archivePath) + L"\" \"" + wideFromUtf8(destFolder) + L"\\\"";
    runner::RunResult extracted = runner::runCaptured(command, destFolder, nullptr);
    if (!extracted.started || extracted.exitCode != 0)
        return fail("WinRAR could not extract the archive.", extracted.output);
    return ok();
}

static Outcome extractWithPowershell(const std::string& archivePath, const std::string& destFolder) {
    std::wstring cmd = L"powershell.exe -NoProfile -Command \"Expand-Archive -LiteralPath '"
        + wideFromUtf8(archivePath) + L"' -DestinationPath '"
        + wideFromUtf8(destFolder) + L"' -Force\"";
    runner::RunResult res = runner::runCaptured(cmd, destFolder, nullptr);
    if (!res.started || res.exitCode != 0)
        return fail("PowerShell Expand-Archive failed.", res.output);
    return ok();
}

Outcome archive::extractArchive(const std::string& archivePath, const std::string& destFolder) {
    Outcome created = paths::ensureFolder(destFolder);
    if (!created.succeeded)
        return created;

    Outcome tar = extractWithTar(archivePath, destFolder);
    if (tar.succeeded)
        return tar;

    Outcome seven = extractWithSevenZip(archivePath, destFolder);
    if (seven.succeeded)
        return seven;

    std::string lower = withoutQuery(archivePath);
    bool isZip = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".zip") == 0;
    if (isZip) {
        Outcome ps = extractWithPowershell(archivePath, destFolder);
        if (ps.succeeded)
            return ps;
        return fail(
            "The archive couldn't be extracted. Tried Windows tar, 7-Zip, and PowerShell Expand-Archive.",
            tar.detail + " | " + seven.detail + " | " + ps.detail
        );
    }

    if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".rar") == 0) {
        Outcome rar = extractWithRar(archivePath, destFolder);
        if (rar.succeeded)
            return rar;
        return fail(
            "That .rar file couldn't extract. Install 7-Zip (or WinRAR), then try again.",
            tar.detail + " " + seven.detail + " " + rar.detail
        );
    }

    return fail(
        "That archive couldn't extract. Only .zip, .rar and .7z are supported, and .rar/.7z need 7-Zip installed.",
        tar.detail + " " + seven.detail
    );
}

std::string archive::unwrapGithubFolder(const std::string& extractedRoot) {
    std::vector<std::string> children = paths::listSubfolders(extractedRoot);
    bool hasIndexHere = paths::hasPluginIndex(extractedRoot);
    if (!hasIndexHere && children.size() == 1)
        return paths::join(extractedRoot, children[0]);
    return extractedRoot;
}

std::string archive::locatePluginSource(const std::string& extractedRoot, const std::string& sourcePath) {
    std::string root = unwrapGithubFolder(extractedRoot);
    if (sourcePath.empty()) {
        if (paths::hasPluginIndex(root))
            return root;
        std::vector<std::string> children = paths::listSubfolders(root);
        if (children.size() == 1 && paths::hasPluginIndex(paths::join(root, children[0])))
            return paths::join(root, children[0]);
        return root;
    }
    if (!paths::isSafeRelativeFile(sourcePath))
        return "";
    return paths::join(root, sourcePath);
}
