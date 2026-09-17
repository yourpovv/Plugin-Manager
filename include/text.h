#pragma once

#include "win32.h"
#include <string>

inline std::wstring wideFromUtf8(const std::string& text) {
    if (text.empty())
        return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    if (count <= 0)
        return L"";
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), wide.data(), count);
    return wide;
}

inline std::string utf8FromWide(const std::wstring& text) {
    if (text.empty())
        return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    if (count <= 0)
        return "";
    std::string utf8(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), utf8.data(), count, nullptr, nullptr);
    return utf8;
}

inline std::string toLowerAscii(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z')
            ch = (char)(ch - 'A' + 'a');
    }
    return text;
}

inline bool startsWith(const std::string& text, const char* prefix) {
    size_t n = 0;
    while (prefix[n])
        n++;
    return text.size() >= n && text.compare(0, n, prefix) == 0;
}

inline bool containsInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty())
        return true;
    return toLowerAscii(haystack).find(toLowerAscii(needle)) != std::string::npos;
}
