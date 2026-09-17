#pragma once

#include "plugin.h"
#include "win32.h"
#include <functional>
#include <string>

namespace ui {

void setCommandHandler(const std::function<void(const std::string&)>& handler);
bool create(HINSTANCE instance);
int runMessageLoop();
HWND mainWindow();

void setEquicordPath(const std::string& path, bool isValid, const std::string& message);
void setCatalog(const std::vector<PluginStatus>& plugins);
void setBusy(bool busy, const std::string& label);
void setProgress(int percent);
void toast(const std::string& message, bool isError);
void refreshLogView();
void showSettings();
void showAbout();
void setSourceText(const std::string& text);
std::string sourceText();
void setDiscordBranch(const std::string& branch);

}
