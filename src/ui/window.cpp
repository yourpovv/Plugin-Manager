#include "window.h"
#include "builder.h"
#include "config.h"
#include "log.h"
#include "pluginList.h"
#include "resource.h"
#include "text.h"
#include "theme.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <mutex>
#include <shellapi.h>
#include <uxtheme.h>
#include <vector>
#include <windowsx.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

namespace {

HWND hwndMain = nullptr;
HWND hwndSettings = nullptr;
HWND hwndAbout = nullptr;
HINSTANCE appInstance = nullptr;
WNDPROC originalButtonProc = nullptr;

std::function<void(const std::string&)> commandHandler;
bool logsOpen = false;
bool filterInstalled = false;
bool filterUpdates = false;
std::string statusText = "Select your Equicord folder to get started.";
bool statusError = false;
int progressPercent = -1;
int initialWidth = 0;
int initialHeight = 0;

struct ButtonLook {
    HWND hwnd = nullptr;
    std::wstring label;
    COLORREF fill = theme::panel;
    COLORREF hover = theme::cardHover;
    COLORREF text = theme::text;
    bool toggle = false;
    bool on = false;
    bool hot = false;
};

struct Anchor {
    int id = 0;
    bool stretchRight = false;
    bool stretchBottom = false;
    bool moveRight = false;
    bool moveBottom = false;
    RECT start{};
};

std::vector<ButtonLook> buttons;
std::vector<Anchor> anchors;

enum { UI_PATH = 1, UI_CATALOG, UI_BUSY, UI_PROGRESS, UI_TOAST };

struct UiNote {
    int kind = 0;
    std::string text;
    std::string detail;
    int percent = 0;
    bool flag = false;
    std::vector<PluginStatus> plugins;
};

std::mutex noteMutex;
std::vector<UiNote> notes;

void applyDarkTitle(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

void pushNote(UiNote note) {
    {
        std::lock_guard<std::mutex> lock(noteMutex);
        notes.push_back(std::move(note));
    }
    if (hwndMain)
        PostMessageW(hwndMain, WM_APP + 1, 0, 0);
}

void sendCommand(const std::string& command) {
    if (commandHandler)
        commandHandler(command);
}

ButtonLook* lookFor(HWND hwnd) {
    for (ButtonLook& button : buttons) {
        if (button.hwnd == hwnd)
            return &button;
    }
    return nullptr;
}

LRESULT CALLBACK ownerButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOUSEMOVE) {
        ButtonLook* look = lookFor(hwnd);
        if (look && !look->hot) {
            look->hot = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&track);
        }
    }
    if (msg == WM_MOUSELEAVE) {
        ButtonLook* look = lookFor(hwnd);
        if (look && look->hot) {
            look->hot = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
    if (msg == WM_SETCURSOR) {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }
    return CallWindowProcW(originalButtonProc, hwnd, msg, wParam, lParam);
}

void styleEdit(HWND hwnd) {
    SetWindowTheme(hwnd, L"", L"");
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style & ~WS_EX_CLIENTEDGE);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)theme::bodyFont(), TRUE);
    SendMessageW(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
}

void addOwnerButton(HWND hwnd, COLORREF fill, COLORREF hover) {
    wchar_t label[64];
    GetWindowTextW(hwnd, label, 64);
    if (!originalButtonProc)
        originalButtonProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)ownerButtonProc);
    else
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)ownerButtonProc);
    ButtonLook look;
    look.hwnd = hwnd;
    look.label = label;
    look.fill = fill;
    look.hover = hover;
    look.text = theme::text;
    buttons.push_back(look);
}

void setToggleOn(int id, bool on) {
    HWND hwnd = GetDlgItem(hwndMain, id);
    for (ButtonLook& button : buttons) {
        if (button.hwnd != hwnd)
            continue;
        button.toggle = true;
        button.on = on;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
}

void setEditText(HWND hwnd, const std::string& text) {
    SetWindowTextW(hwnd, wideFromUtf8(text).c_str());
}

std::string getEditText(HWND hwnd) {
    int length = GetWindowTextLengthW(hwnd);
    std::wstring wide(length, L'\0');
    GetWindowTextW(hwnd, wide.data(), length + 1);
    return utf8FromWide(wide);
}

void applyFilter() {
    pluginList::setFilter(getEditText(GetDlgItem(hwndMain, IDC_SEARCH)), filterInstalled, filterUpdates);
}

void captureAnchors(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    initialWidth = client.right;
    initialHeight = client.bottom;
    Anchor specs[] = {
        { IDC_PATH, true, false, false, false },
        { IDC_DISCORD_BRANCH, false, false, true, false },
        { IDC_BROWSE, false, false, true, false },
        { IDC_SOURCE, true, false, false, false },
        { IDC_BROWSE_PLUGIN, false, false, true, false },
        { IDC_INSTALL_SOURCE, false, false, true, false },
        { IDC_DROP_HINT, true, false, false, false },
        { IDC_SEARCH, true, false, false, false },
        { IDC_REFRESH, false, false, true, false },
        { IDC_FILTER_INSTALLED, false, false, true, false },
        { IDC_FILTER_UPDATES, false, false, true, false },
        { IDC_SELECT_ALL, false, false, true, false },
        { IDC_PLUGIN_LIST, true, true, false, false },
        { IDC_LOG_VIEW, true, false, false, true },
        { IDC_INSTALL_SELECTED, false, false, false, true },
        { IDC_BUILD, false, false, false, true },
        { IDC_LOGS, false, false, true, true },
        { IDC_SETTINGS, false, false, true, true },
        { IDC_ABOUT, false, false, true, true },
        { IDC_STATUS, true, false, false, true },
    };
    anchors.clear();
    for (Anchor spec : specs) {
        HWND child = GetDlgItem(hwnd, spec.id);
        if (!child)
            continue;
        GetWindowRect(child, &spec.start);
        MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&spec.start, 2);
        anchors.push_back(spec);
    }
}

void applyAnchors(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    int deltaX = client.right - initialWidth;
    int deltaY = client.bottom - initialHeight;
    for (const Anchor& anchor : anchors) {
        RECT rc = anchor.start;
        if (anchor.stretchRight)
            rc.right += deltaX;
        if (anchor.stretchBottom)
            rc.bottom += deltaY;
        if (anchor.moveRight) {
            rc.left += deltaX;
            rc.right += deltaX;
        }
        if (anchor.moveBottom) {
            rc.top += deltaY;
            rc.bottom += deltaY;
        }
        MoveWindow(GetDlgItem(hwnd, anchor.id), rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }
}

void paintOwnerButton(const DRAWITEMSTRUCT* item) {
    ButtonLook* look = lookFor(item->hwndItem);
    if (!look)
        return;
    bool pressed = item->itemState & ODS_SELECTED;
    COLORREF fill = look->fill;
    if (look->toggle && look->on)
        fill = theme::accent;
    else if (pressed)
        fill = theme::accentPress;
    else if (look->hot)
        fill = look->hover;
    theme::fillRound(item->hDC, item->rcItem, fill, 8);
    COLORREF text = (look->toggle && look->on) ? theme::text : look->text;
    theme::drawText(item->hDC, item->rcItem, look->label.c_str(), text, theme::buttonFont(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void setBusyVisual(bool on) {
    pluginList::setEnabled(!on);
    EnableWindow(GetDlgItem(hwndMain, IDC_BROWSE), !on);
    EnableWindow(GetDlgItem(hwndMain, IDC_BROWSE_PLUGIN), !on);
    EnableWindow(GetDlgItem(hwndMain, IDC_INSTALL_SOURCE), !on);
    EnableWindow(GetDlgItem(hwndMain, IDC_REFRESH), !on);
    EnableWindow(GetDlgItem(hwndMain, IDC_INSTALL_SELECTED), !on);
    EnableWindow(GetDlgItem(hwndMain, IDC_BUILD), !on);
}

void syncLogView() {
    HWND log = GetDlgItem(hwndMain, IDC_LOG_VIEW);
    if (!log)
        return;
    std::string text = appLog::text();
    setEditText(log, text);
    SendMessageW(log, EM_SETSEL, (WPARAM)text.size(), (LPARAM)text.size());
    SendMessageW(log, EM_SCROLLCARET, 0, 0);
}

void applyNote(const UiNote& note) {
    if (note.kind == UI_PATH) {
        setEditText(GetDlgItem(hwndMain, IDC_PATH), note.text);
        statusText = note.detail;
        statusError = !note.flag && !note.text.empty();
        SetDlgItemTextW(hwndMain, IDC_STATUS, wideFromUtf8(statusText).c_str());
        return;
    }
    if (note.kind == UI_CATALOG) {
        pluginList::setCards(note.plugins);
        applyFilter();
        return;
    }
    if (note.kind == UI_BUSY) {
        setBusyVisual(note.flag);
        if (!note.text.empty()) {
            statusText = note.text;
            statusError = false;
            SetDlgItemTextW(hwndMain, IDC_STATUS, wideFromUtf8(statusText).c_str());
        }
        if (!note.flag)
            progressPercent = -1;
        return;
    }
    if (note.kind == UI_PROGRESS) {
        progressPercent = note.percent;
        return;
    }
    if (note.kind == UI_TOAST) {
        statusText = note.text;
        statusError = note.flag;
        SetDlgItemTextW(hwndMain, IDC_STATUS, wideFromUtf8(statusText).c_str());
    }
}

void drainNotes() {
    std::vector<UiNote> local;
    {
        std::lock_guard<std::mutex> lock(noteMutex);
        local.swap(notes);
    }
    for (const UiNote& note : local)
        applyNote(note);
    InvalidateRect(hwndMain, nullptr, FALSE);
}

void fillDiscordCombo(HWND hwnd) {
    HWND combo = GetDlgItem(hwnd, IDC_DISCORD_BRANCH);
    if (!combo)
        return;
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(combo, WM_SETFONT, (WPARAM)theme::bodyFont(), TRUE);
    int selected = 0;
    std::vector<builder::DiscordChannel> channels = builder::discordChannels();
    for (int i = 0; i < (int)channels.size(); i++) {
        std::wstring label = wideFromUtf8(channels[i].label);
        if (!channels[i].installed)
            label += L" (not installed)";
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)label.c_str());
        if (channels[i].branch == cfg::discordBranch)
            selected = i;
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

void wireControls(HWND hwnd) {
    styleEdit(GetDlgItem(hwnd, IDC_PATH));
    styleEdit(GetDlgItem(hwnd, IDC_SOURCE));
    styleEdit(GetDlgItem(hwnd, IDC_SEARCH));
    styleEdit(GetDlgItem(hwnd, IDC_LOG_VIEW));
    SendMessageW(GetDlgItem(hwnd, IDC_SOURCE), EM_SETCUEBANNER, TRUE, (LPARAM)L"https://github.com/user/repo/tree/main/myPlugin");
    SendMessageW(GetDlgItem(hwnd, IDC_SEARCH), EM_SETCUEBANNER, TRUE, (LPARAM)L"Search");

    addOwnerButton(GetDlgItem(hwnd, IDC_BROWSE), theme::accent, theme::accentHover);
    addOwnerButton(GetDlgItem(hwnd, IDC_BROWSE_PLUGIN), theme::panel, theme::cardHover);
    addOwnerButton(GetDlgItem(hwnd, IDC_INSTALL_SOURCE), theme::accent, theme::accentHover);
    addOwnerButton(GetDlgItem(hwnd, IDC_REFRESH), theme::panel, theme::cardHover);
    HWND installed = GetDlgItem(hwnd, IDC_FILTER_INSTALLED);
    addOwnerButton(installed, theme::panel, theme::cardHover);
    lookFor(installed)->toggle = true;
    HWND updates = GetDlgItem(hwnd, IDC_FILTER_UPDATES);
    addOwnerButton(updates, theme::panel, theme::cardHover);
    lookFor(updates)->toggle = true;
    addOwnerButton(GetDlgItem(hwnd, IDC_SELECT_ALL), theme::panel, theme::cardHover);
    addOwnerButton(GetDlgItem(hwnd, IDC_INSTALL_SELECTED), theme::accent, theme::accentHover);
    addOwnerButton(GetDlgItem(hwnd, IDC_BUILD), theme::panel, theme::cardHover);
    HWND logs = GetDlgItem(hwnd, IDC_LOGS);
    addOwnerButton(logs, theme::panel, theme::cardHover);
    lookFor(logs)->toggle = true;
    addOwnerButton(GetDlgItem(hwnd, IDC_SETTINGS), theme::panel, theme::cardHover);
    addOwnerButton(GetDlgItem(hwnd, IDC_ABOUT), theme::panel, theme::cardHover);

    fillDiscordCombo(hwnd);
    pluginList::attach(GetDlgItem(hwnd, IDC_PLUGIN_LIST));
    pluginList::setActionHandler(sendCommand);
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(appInstance, MAKEINTRESOURCE(IDI_APP)));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(appInstance, MAKEINTRESOURCE(IDI_APP)));
    DragAcceptFiles(hwnd, TRUE);
}

void onCommand(int id) {
    switch (id) {
        case IDC_BROWSE: sendCommand("browse"); break;
        case IDC_BROWSE_PLUGIN: sendCommand("browse-plugin"); break;
        case IDC_INSTALL_SOURCE: sendCommand("install-source:" + getEditText(GetDlgItem(hwndMain, IDC_SOURCE))); break;
        case IDC_REFRESH: sendCommand("refresh"); break;
        case IDC_FILTER_INSTALLED:
            filterInstalled = !filterInstalled;
            setToggleOn(IDC_FILTER_INSTALLED, filterInstalled);
            applyFilter();
            break;
        case IDC_FILTER_UPDATES:
            filterUpdates = !filterUpdates;
            setToggleOn(IDC_FILTER_UPDATES, filterUpdates);
            applyFilter();
            break;
        case IDC_SELECT_ALL: pluginList::selectAllVisible(); break;
        case IDC_INSTALL_SELECTED: {
            std::vector<std::string> ids = pluginList::selectedIds();
            if (ids.empty()) {
                ui::toast("Select one or more of your catalog plugins first.", true);
                break;
            }
            std::string command = "install:";
            for (size_t i = 0; i < ids.size(); i++) {
                if (i) command += ",";
                command += ids[i];
            }
            sendCommand(command);
            break;
        }
        case IDC_BUILD: sendCommand("rebuild"); break;
        case IDC_LOGS:
            logsOpen = !logsOpen;
            setToggleOn(IDC_LOGS, logsOpen);
            ShowWindow(GetDlgItem(hwndMain, IDC_LOG_VIEW), logsOpen ? SW_SHOW : SW_HIDE);
            syncLogView();
            break;
        case IDC_SETTINGS: ui::showSettings(); break;
        case IDC_ABOUT: ui::showAbout(); break;
        default: break;
    }
}

void onDropFiles(HDROP drop) {
    UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; i++) {
        wchar_t path[MAX_PATH];
        if (!DragQueryFileW(drop, i, path, MAX_PATH))
            continue;
        std::string utf8 = utf8FromWide(path);
        setEditText(GetDlgItem(hwndMain, IDC_SOURCE), utf8);
        sendCommand("install-source:" + utf8);
    }
    DragFinish(drop);
}

INT_PTR CALLBACK mainDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            hwndMain = hwnd;
            applyDarkTitle(hwnd);
            wireControls(hwnd);
            captureAnchors(hwnd);
            return TRUE;
        case WM_SIZE:
            if (initialWidth)
                applyAnchors(hwnd);
            return TRUE;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* info = (MINMAXINFO*)lParam;
            info->ptMinTrackSize = { 820, 560 };
            return TRUE;
        }
        case WM_DRAWITEM:
            paintOwnerButton((DRAWITEMSTRUCT*)lParam);
            return TRUE;
        case WM_CTLCOLORDLG:
            return (INT_PTR)theme::backgroundBrush();
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, theme::text);
            SetBkColor(dc, theme::input);
            return (INT_PTR)theme::inputBrush();
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wParam;
            HWND child = (HWND)lParam;
            SetBkColor(dc, theme::background);
            if (child == GetDlgItem(hwnd, IDC_STATUS) && statusError)
                SetTextColor(dc, theme::danger);
            else if (child == GetDlgItem(hwnd, IDC_TITLE))
                SetTextColor(dc, theme::text);
            else
                SetTextColor(dc, theme::muted);
            return (INT_PTR)theme::backgroundBrush();
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_SEARCH) {
                applyFilter();
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_DISCORD_BRANCH && HIWORD(wParam) == CBN_SELCHANGE) {
                int index = (int)SendMessageW(GetDlgItem(hwnd, IDC_DISCORD_BRANCH), CB_GETCURSEL, 0, 0);
                std::vector<builder::DiscordChannel> channels = builder::discordChannels();
                if (index >= 0 && index < (int)channels.size())
                    sendCommand("discord-branch:" + channels[index].branch);
                return TRUE;
            }
            if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == 0)
                onCommand(LOWORD(wParam));
            return TRUE;
        case WM_DROPFILES:
            onDropFiles((HDROP)wParam);
            return TRUE;
        case WM_APP:
            syncLogView();
            return TRUE;
        case WM_APP + 1:
            syncLogView();
            drainNotes();
            return TRUE;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return TRUE;
        case WM_DESTROY:
            PostQuitMessage(0);
            return TRUE;
        default:
            return FALSE;
    }
}

INT_PTR CALLBACK settingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            applyDarkTitle(hwnd);
            SetDlgItemTextW(hwnd, IDC_SETTINGS_URL, wideFromUtf8(cfg::manifestUrl).c_str());
            CheckDlgButton(hwnd, IDC_SETTINGS_AUTO, cfg::autoBuild ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_SETTINGS_DEV, cfg::buildDev ? BST_CHECKED : BST_UNCHECKED);
            styleEdit(GetDlgItem(hwnd, IDC_SETTINGS_URL));
            return TRUE;
        case WM_CTLCOLORDLG:
            return (INT_PTR)theme::backgroundBrush();
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, theme::text);
            SetBkColor(dc, theme::input);
            return (INT_PTR)theme::inputBrush();
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, theme::muted);
            SetBkColor(dc, theme::background);
            return (INT_PTR)theme::backgroundBrush();
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, 0);
                return TRUE;
            }
            if (LOWORD(wParam) == IDOK) {
                std::string url = getEditText(GetDlgItem(hwnd, IDC_SETTINGS_URL));
                bool autoBuild = IsDlgButtonChecked(hwnd, IDC_SETTINGS_AUTO) == BST_CHECKED;
                bool dev = IsDlgButtonChecked(hwnd, IDC_SETTINGS_DEV) == BST_CHECKED;
                sendCommand("settings:" + url + "\n" + (autoBuild ? "1" : "0") + (dev ? "1" : "0"));
                EndDialog(hwnd, 1);
                return TRUE;
            }
            return TRUE;
        default:
            return FALSE;
    }
}

INT_PTR CALLBACK aboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            applyDarkTitle(hwnd);
            return TRUE;
        case WM_CTLCOLORDLG:
            return (INT_PTR)theme::backgroundBrush();
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, theme::muted);
            SetBkColor(dc, theme::background);
            return (INT_PTR)theme::backgroundBrush();
        }
        case WM_COMMAND:
            EndDialog(hwnd, 0);
            return TRUE;
        default:
            return FALSE;
    }
}

}

void ui::setCommandHandler(const std::function<void(const std::string&)>& handler) {
    commandHandler = handler;
}

bool ui::create(HINSTANCE instance) {
    appInstance = instance;
    theme::startup();
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);
    pluginList::registerClass(instance);

    hwndMain = CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_MAIN), nullptr, mainDlgProc, 0);
    if (!hwndMain)
        return false;
    ShowWindow(hwndMain, SW_SHOW);
    UpdateWindow(hwndMain);
    return true;
}

int ui::runMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (hwndMain && IsDialogMessageW(hwndMain, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    theme::shutdown();
    return (int)msg.wParam;
}

HWND ui::mainWindow() {
    return hwndMain;
}

void ui::setEquicordPath(const std::string& path, bool isValid, const std::string& message) {
    UiNote note;
    note.kind = UI_PATH;
    note.text = path;
    note.detail = message;
    note.flag = isValid;
    pushNote(note);
}

void ui::setCatalog(const std::vector<PluginStatus>& plugins) {
    UiNote note;
    note.kind = UI_CATALOG;
    note.plugins = plugins;
    pushNote(note);
}

void ui::setBusy(bool on, const std::string& label) {
    UiNote note;
    note.kind = UI_BUSY;
    note.flag = on;
    note.text = label;
    pushNote(note);
}

void ui::setProgress(int percent) {
    UiNote note;
    note.kind = UI_PROGRESS;
    note.percent = percent;
    pushNote(note);
}

void ui::toast(const std::string& message, bool isError) {
    UiNote note;
    note.kind = UI_TOAST;
    note.text = message;
    note.flag = isError;
    pushNote(note);
}

void ui::refreshLogView() {
    syncLogView();
}

void ui::showSettings() {
    DialogBoxParamW(appInstance, MAKEINTRESOURCEW(IDD_SETTINGS), hwndMain, settingsDlgProc, 0);
}

void ui::showAbout() {
    DialogBoxParamW(appInstance, MAKEINTRESOURCEW(IDD_ABOUT), hwndMain, aboutDlgProc, 0);
}

void ui::setSourceText(const std::string& text) {
    if (hwndMain)
        setEditText(GetDlgItem(hwndMain, IDC_SOURCE), text);
}

std::string ui::sourceText() {
    if (!hwndMain)
        return "";
    return getEditText(GetDlgItem(hwndMain, IDC_SOURCE));
}

void ui::setDiscordBranch(const std::string& branch) {
    if (!hwndMain)
        return;
    HWND combo = GetDlgItem(hwndMain, IDC_DISCORD_BRANCH);
    std::vector<builder::DiscordChannel> channels = builder::discordChannels();
    for (int i = 0; i < (int)channels.size(); i++) {
        if (channels[i].branch == branch) {
            SendMessageW(combo, CB_SETCURSEL, i, 0);
            return;
        }
    }
}
