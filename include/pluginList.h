#pragma once

#include "plugin.h"
#include "win32.h"
#include <functional>
#include <string>
#include <vector>

namespace pluginList {

void registerClass(HINSTANCE instance);
void attach(HWND hwnd);
void setCards(const std::vector<PluginStatus>& plugins);
void setFilter(const std::string& query, bool installedOnly, bool updatesOnly);
void setActionHandler(const std::function<void(const std::string&)>& handler);
void setEnabled(bool enabled);
std::vector<std::string> selectedIds();
void selectAllVisible();
void move(int x, int y, int width, int height);

}
