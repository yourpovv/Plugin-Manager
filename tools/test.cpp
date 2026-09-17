#include "check.h"
#include "archive.h"
#include "json.h"
#include "paths.h"
#include "plugin.h"

int main() {
    const char* catalog = R"({
      "version": 1,
      "command": "rm -rf /",
      "plugins": [
        {
          "id": "usernameSpoofer",
          "name": "Username spoofer",
          "description": "A plugin to larp with, this does nothing at all :)",
          "author": "yourpov",
          "version": "1.2.3",
          "download": "https://example.com/plugin.zip",
          "exec": "larp.exe",
          "files": [
            { "url": "https://yourpov.dev/files/help.txt", "path": "index.ts" }
          ]
        }
      ]
    })";

    check(json::getInt(catalog, "version") == 1);
    check(json::getString(catalog, "command") == "rm -rf /");
    check(json::getObjectArray(catalog, "plugins").size() == 1);

    std::string plugin = json::getObjectArray(catalog, "plugins")[0];
    check(json::getString(plugin, "id") == "examplePlugin");
    check(json::getString(plugin, "download") == "https://example.com/plugin.zip");
    check(json::getString(plugin, "exec") == "calc.exe");
    check(json::getObjectArray(plugin, "files").size() == 1);
    check(json::getString(json::getObjectArray(plugin, "files")[0], "path") == "index.ts");

    check(json::escape("a\"b\\c") == "a\\\"b\\\\c");
    check(json::getString("{}", "missing") == "");
    check(json::getInt("{}", "width", 9) == 9);
    check(json::getBool("{\"autoBuild\": true}", "autoBuild") == true);
    check(json::getBool("{\"autoBuild\": false}", "autoBuild") == false);

    check(paths::isSafePluginId("examplePlugin"));
    check(paths::isSafePluginId("dm-read-receipt"));
    check(!paths::isSafePluginId(""));
    check(!paths::isSafePluginId("../evil"));
    check(!paths::isSafePluginId("evil/plugin"));
    check(!paths::isSafePluginId("C:hack"));
    check(!paths::isSafePluginId("1bad"));

    check(paths::isSafeArchiveEntry("repo-main/index.ts"));
    check(!paths::isSafeArchiveEntry("../evil.ts"));
    check(!paths::isSafeArchiveEntry("foo/../../windows/system32/evil.ts"));
    check(!paths::isSafeArchiveEntry("/etc/passwd"));
    check(!paths::isSafeArchiveEntry("C:\\Windows\\notepad.exe"));
    check(!paths::isSafeArchiveEntry(""));
    check(json::getString("{\"a\":\"\\u0041x\"}", "a") == "?x");
    check(paths::isSafeRelativeFile("index.tsx"));
    check(!paths::isSafeRelativeFile("../index.ts"));
    check(!paths::isSafeRelativeFile("nested/../../x.ts"));

    check(plugins::versionIsNewer("1.2.0", "1.1.9"));
    check(plugins::versionIsNewer("2.0.0", "1.9.9"));
    check(!plugins::versionIsNewer("1.0.0", "1.0.0"));
    check(plugins::versionIsNewer("1.0.0", ""));

    CatalogPlugin fromGithub = plugins::fromSource("https://github.com/yourpov/Equicord/tree/main/src/equicordplugins/streamProof");
    check(fromGithub.id == "streamProof");
    check(fromGithub.sourcePath == "src/equicordplugins/streamProof");
    CatalogPlugin fromZip = plugins::fromSource("C:\\mods\\coolPlugin.zip");
    check(fromZip.id == "coolPlugin");
    CatalogPlugin fromRar = plugins::fromSource("C:\\mods\\coolPlugin.rar");
    check(fromRar.id == "coolPlugin");
    check(archive::isArchivePath("plugin.7z"));
    check(archive::isArchivePath("https://example.com/mod.tar.gz"));
    check(archive::isArchiveBytes(std::string("PK\x03\x04xx", 6)));
    check(archive::isArchiveBytes(std::string("Rar!\x1a\x07\x00", 7)));
    check(paths::isPluginSourceFile("index.ts"));
    check(paths::isPluginSourceFile("style.CSS"));
    check(paths::isPluginSourceFile("index.tsx"));
    check(!paths::isPluginSourceFile("readme.md"));
    check(!paths::isPluginSourceFile("native.js"));

    printf(testFailures ? "%d failed\n" : "all passed\n", testFailures);
    return testFailures ? 1 : 0;
}
