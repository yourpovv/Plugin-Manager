#include "pluginList.h"
#include "text.h"
#include "theme.h"
#include <algorithm>
#include <windowsx.h>

namespace {

constexpr int cardHeight = 84;
constexpr int cardGap = 10;
constexpr int buttonWidth = 88;
constexpr int buttonHeight = 30;

struct CardHit {
    RECT card{};
    RECT checkbox{};
    RECT primary{};
    RECT removeBtn{};
    int index = -1;
};

enum HotPart { HotNone, HotCard, HotPrimary, HotRemove };

struct PluginListState {
    HWND hwnd = nullptr;
    std::vector<PluginStatus> all;
    std::vector<char> selected;
    std::vector<int> visible;
    std::vector<CardHit> hits;
    std::string query;
    bool installedOnly = false;
    bool updatesOnly = false;
    bool enabled = true;
    int hoverIndex = -1;
    HotPart hoverPart = HotNone;
    int scroll = 0;
    std::function<void(const std::string&)> action;
};

PluginListState state;

bool cardMatches(const PluginStatus& plugin) {
    if (state.installedOnly && !plugin.isInstalled)
        return false;
    if (state.updatesOnly && !plugin.updateAvailable)
        return false;
    if (state.query.empty())
        return true;
    const CatalogPlugin& item = plugin.catalog;
    return containsInsensitive(item.name, state.query)
        || containsInsensitive(item.description, state.query)
        || containsInsensitive(item.author, state.query)
        || containsInsensitive(item.id, state.query);
}

void rebuildVisible() {
    state.visible.clear();
    for (int i = 0; i < (int)state.all.size(); i++) {
        if (cardMatches(state.all[i]))
            state.visible.push_back(i);
    }
}

int contentHeight() {
    int count = (int)state.visible.size();
    if (count == 0)
        return 80;
    return count * (cardHeight + cardGap);
}

void updateScrollBar(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = std::max(0, contentHeight());
    info.nPage = (UINT)std::max(1L, rc.bottom);
    info.nPos = state.scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
    int maxScroll = std::max(0, (int)info.nMax - (int)info.nPage);
    if (state.scroll > maxScroll)
        state.scroll = maxScroll;
}

void paintEmpty(HDC dc, RECT rc, const wchar_t* title, const wchar_t* body) {
    RECT titleRc = rc;
    titleRc.top += 28;
    titleRc.bottom = titleRc.top + 24;
    theme::drawText(dc, titleRc, title, theme::text, theme::headingFont(), DT_CENTER | DT_SINGLELINE);
    RECT bodyRc = rc;
    bodyRc.top = titleRc.bottom + 6;
    bodyRc.left += 24;
    bodyRc.right -= 24;
    theme::drawText(dc, bodyRc, body, theme::muted, theme::bodyFont(), DT_CENTER | DT_WORDBREAK);
}

bool pointIn(const RECT& rc, POINT p) {
    return PtInRect(&rc, p);
}

void paintButton(HDC dc, RECT rc, const wchar_t* label, COLORREF fill, COLORREF text, bool hot) {
    COLORREF color = fill;
    if (hot && fill == theme::accent)
        color = theme::accentHover;
    else if (hot && fill == theme::danger)
        color = theme::dangerHover;
    else if (hot)
        color = theme::cardHover;
    theme::fillRound(dc, rc, color, 8);
    theme::drawText(dc, rc, label, text, theme::buttonFont(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paintCheckbox(HDC dc, RECT rc, bool on) {
    theme::fillRound(dc, rc, on ? theme::accent : theme::input, 4);
    theme::strokeRound(dc, rc, on ? theme::accent : theme::border, 4);
    if (on)
        theme::drawText(dc, rc, L"✓", theme::text, theme::smallFont(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paintStatusPill(HDC dc, RECT card, const PluginStatus& plugin) {
    const wchar_t* badgeText = L"Not installed";
    COLORREF fill = RGB(67, 70, 77);
    if (plugin.updateAvailable) {
        badgeText = L"Update";
        fill = RGB(88, 67, 32);
    } else if (plugin.isInstalled) {
        badgeText = L"Installed";
        fill = RGB(26, 77, 46);
    }
    RECT pill{ card.right - 118, card.top + 12, card.right - 16, card.top + 30 };
    theme::fillPill(dc, pill, fill);
    COLORREF label = plugin.updateAvailable ? theme::warning : (plugin.isInstalled ? theme::success : theme::muted);
    theme::drawText(dc, pill, badgeText, label, theme::smallFont(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paintCard(HDC dc, const PluginStatus& plugin, bool selected, bool hover, HotPart hotPart, CardHit& hit) {
    COLORREF back = (hover || selected) ? theme::cardHover : theme::card;
    theme::fillRound(dc, hit.card, back, 10);
    theme::strokeRound(dc, hit.card, selected ? theme::accent : theme::border, 10);
    if (selected) {
        RECT bar = hit.card;
        bar.right = bar.left + 4;
        theme::fillRound(dc, bar, theme::accent, 4);
    }

    paintCheckbox(dc, hit.checkbox, selected);
    paintStatusPill(dc, hit.card, plugin);

    RECT nameRc = hit.card;
    nameRc.left = hit.checkbox.right + 12;
    nameRc.top += 10;
    nameRc.right = hit.card.right - 130;
    nameRc.bottom = nameRc.top + 20;
    theme::drawText(dc, nameRc, wideFromUtf8(plugin.catalog.name).c_str(), theme::text, theme::headingFont(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT descRc = nameRc;
    descRc.top = nameRc.bottom + 2;
    descRc.bottom = descRc.top + 18;
    descRc.right = hit.primary.left - 12;
    theme::drawText(dc, descRc, wideFromUtf8(plugin.catalog.description).c_str(), theme::muted, theme::bodyFont(), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT metaRc = hit.card;
    metaRc.left = nameRc.left;
    metaRc.top = hit.card.bottom - 24;
    metaRc.bottom = hit.card.bottom - 8;
    metaRc.right = hit.primary.left - 12;
    std::string by = "by " + plugin.catalog.author + "  ·  v" + plugin.catalog.version;
    if (plugin.isInstalled && !plugin.installedVersion.empty())
        by += "  ·  installed " + plugin.installedVersion;
    theme::drawText(dc, metaRc, wideFromUtf8(by).c_str(), theme::muted, theme::smallFont(), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    bool primaryHot = hover && hotPart == HotPrimary;
    bool removeHot = hover && hotPart == HotRemove;
    if (plugin.isInstalled && plugin.updateAvailable)
        paintButton(dc, hit.primary, L"Update", theme::accent, theme::text, primaryHot);
    else if (plugin.isInstalled)
        paintButton(dc, hit.primary, L"Installed", RGB(67, 70, 77), theme::muted, false);
    else
        paintButton(dc, hit.primary, L"Install", theme::accent, theme::text, primaryHot);

    if (plugin.isInstalled)
        paintButton(dc, hit.removeBtn, L"Remove", theme::danger, theme::text, removeHot);
}

void paint(HWND hwnd) {
    PAINTSTRUCT paintStruct;
    HDC dc = BeginPaint(hwnd, &paintStruct);
    RECT rc;
    GetClientRect(hwnd, &rc);

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(mem, bitmap);
    FillRect(mem, &rc, theme::backgroundBrush());

    state.hits.clear();
    if (state.visible.empty()) {
        if (state.all.empty())
            paintEmpty(mem, rc, L"No plugins yet", L"Paste a GitHub folder link, pick a local folder or zip, or drop a plugin on this window.");
        else
            paintEmpty(mem, rc, L"No plugins match", L"Clear search or turn off the Installed / Updates filters.");
    } else {
        int y = 4 - state.scroll;
        for (int visibleIndex = 0; visibleIndex < (int)state.visible.size(); visibleIndex++) {
            int index = state.visible[visibleIndex];
            CardHit hit;
            hit.index = index;
            hit.card = { 0, y, rc.right - 14, y + cardHeight };
            int mid = hit.card.top + (cardHeight - buttonHeight) / 2;
            hit.checkbox = { hit.card.left + 16, hit.card.top + 18, hit.card.left + 34, hit.card.top + 36 };
            hit.primary = { hit.card.right - 16 - buttonWidth, mid, hit.card.right - 16, mid + buttonHeight };
            bool showRemove = state.all[index].isInstalled;
            if (showRemove)
                hit.removeBtn = { hit.primary.left - 8 - buttonWidth, hit.primary.top, hit.primary.left - 8, hit.primary.bottom };
            else
                hit.removeBtn = { 0, 0, 0, 0 };
            if (hit.card.bottom >= 0 && hit.card.top <= rc.bottom) {
                bool selected = index < (int)state.selected.size() && state.selected[index];
                HotPart part = state.hoverIndex == index ? state.hoverPart : HotNone;
                paintCard(mem, state.all[index], selected, state.hoverIndex == index, part, hit);
            }
            state.hits.push_back(hit);
            y += cardHeight + cardGap;
        }
    }

    BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bitmap);
    DeleteObject(mem);
    EndPaint(hwnd, &paintStruct);
}

int hitIndex(POINT p, RECT CardHit::* part) {
    for (const CardHit& hit : state.hits) {
        if (pointIn(hit.*part, p))
            return hit.index;
    }
    return -1;
}

void toggleSelected(int index) {
    if (index < 0 || index >= (int)state.selected.size())
        return;
    state.selected[index] = !state.selected[index];
    InvalidateRect(state.hwnd, nullptr, FALSE);
}

void fire(const std::string& command) {
    if (!state.enabled)
        return;
    if (state.action)
        state.action(command);
}

void onClick(POINT p) {
    int check = hitIndex(p, &CardHit::checkbox);
    if (check >= 0) {
        toggleSelected(check);
        return;
    }
    int removeAt = hitIndex(p, &CardHit::removeBtn);
    if (removeAt >= 0 && state.all[removeAt].isInstalled) {
        fire("remove:" + state.all[removeAt].catalog.id);
        return;
    }
    int primary = hitIndex(p, &CardHit::primary);
    if (primary >= 0) {
        const PluginStatus& plugin = state.all[primary];
        if (plugin.isInstalled && !plugin.updateAvailable)
            return;
        fire("install:" + plugin.catalog.id);
        return;
    }
    int card = hitIndex(p, &CardHit::card);
    if (card >= 0)
        toggleSelected(card);
}

void onScroll(HWND hwnd, int request, int track) {
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &info);
    int pos = info.nPos;
    switch (request) {
        case SB_LINEUP: pos -= 40; break;
        case SB_LINEDOWN: pos += 40; break;
        case SB_PAGEUP: pos -= (int)info.nPage; break;
        case SB_PAGEDOWN: pos += (int)info.nPage; break;
        case SB_THUMBTRACK: pos = track; break;
        default: break;
    }
    int maxScroll = std::max(0, info.nMax - (int)info.nPage);
    if (pos < 0) pos = 0;
    if (pos > maxScroll) pos = maxScroll;
    state.scroll = pos;
    SetScrollPos(hwnd, SB_VERT, pos, TRUE);
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK pluginListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            paint(hwnd);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            updateScrollBar(hwnd);
            return 0;
        case WM_MOUSEMOVE: {
            POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int hover = hitIndex(p, &CardHit::card);
            HotPart part = HotNone;
            if (hover >= 0) {
                if (hitIndex(p, &CardHit::primary) == hover)
                    part = HotPrimary;
                else if (hitIndex(p, &CardHit::removeBtn) == hover)
                    part = HotRemove;
                else
                    part = HotCard;
            }
            if (hover != state.hoverIndex || part != state.hoverPart) {
                state.hoverIndex = hover;
                state.hoverPart = part;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&track);
            return 0;
        }
        case WM_MOUSELEAVE:
            state.hoverIndex = -1;
            state.hoverPart = HotNone;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            onClick(p);
            return 0;
        }
        case WM_MOUSEWHEEL:
            onScroll(hwnd, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            return 0;
        case WM_VSCROLL:
            onScroll(hwnd, LOWORD(wParam), HIWORD(wParam));
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

}

void pluginList::registerClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = pluginListProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"EpmPluginList";
    RegisterClassExW(&windowClass);
}

void pluginList::attach(HWND hwnd) {
    state.hwnd = hwnd;
}

void pluginList::setCards(const std::vector<PluginStatus>& plugins) {
    state.all = plugins;
    state.selected.assign(plugins.size(), 0);
    rebuildVisible();
    state.scroll = 0;
    if (state.hwnd) {
        updateScrollBar(state.hwnd);
        InvalidateRect(state.hwnd, nullptr, FALSE);
    }
}

void pluginList::setFilter(const std::string& query, bool installedOnly, bool updatesOnly) {
    state.query = query;
    state.installedOnly = installedOnly;
    state.updatesOnly = updatesOnly;
    rebuildVisible();
    state.scroll = 0;
    if (state.hwnd) {
        updateScrollBar(state.hwnd);
        InvalidateRect(state.hwnd, nullptr, FALSE);
    }
}

void pluginList::setActionHandler(const std::function<void(const std::string&)>& handler) {
    state.action = handler;
}

void pluginList::setEnabled(bool enabled) {
    state.enabled = enabled;
}

std::vector<std::string> pluginList::selectedIds() {
    std::vector<std::string> ids;
    for (int i = 0; i < (int)state.all.size(); i++) {
        if (i < (int)state.selected.size() && state.selected[i])
            ids.push_back(state.all[i].catalog.id);
    }
    return ids;
}

void pluginList::selectAllVisible() {
    bool allOn = true;
    for (int index : state.visible) {
        if (!state.selected[index])
            allOn = false;
    }
    for (int index : state.visible)
        state.selected[index] = allOn ? 0 : 1;
    if (state.hwnd)
        InvalidateRect(state.hwnd, nullptr, FALSE);
}

void pluginList::move(int x, int y, int width, int height) {
    if (state.hwnd)
        MoveWindow(state.hwnd, x, y, width, height, TRUE);
}
