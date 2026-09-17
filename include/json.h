#pragma once

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace json {

inline void skipSpace(const std::string& raw, size_t& index) {
    while (index < raw.size() && (unsigned char)raw[index] <= ' ')
        index++;
}

inline bool parseString(const std::string& raw, size_t& index, std::string& out) {
    skipSpace(raw, index);
    if (index >= raw.size() || raw[index] != '"')
        return false;
    index++;
    out.clear();
    while (index < raw.size()) {
        char ch = raw[index++];
        if (ch == '"')
            return true;
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (index >= raw.size())
            return false;
        char escaped = raw[index++];
        switch (escaped) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                if (index + 4 > raw.size())
                    return false;
                index += 4;
                out.push_back('?');
                break;
            default:
                out.push_back(escaped);
                break;
        }
    }
    return false;
}

inline bool skipValue(const std::string& raw, size_t& index);

inline bool skipContainer(const std::string& raw, size_t& index) {
    skipSpace(raw, index);
    if (index >= raw.size())
        return false;
    char open = raw[index];
    if (open != '{' && open != '[')
        return false;
    index++;
    int depth = 1;
    bool inString = false;
    bool escape = false;
    while (index < raw.size() && depth > 0) {
        char ch = raw[index++];
        if (inString) {
            if (escape)
                escape = false;
            else if (ch == '\\')
                escape = true;
            else if (ch == '"')
                inString = false;
            continue;
        }
        if (ch == '"')
            inString = true;
        else if (ch == '{' || ch == '[')
            depth++;
        else if (ch == '}' || ch == ']')
            depth--;
    }
    return depth == 0;
}

inline bool skipValue(const std::string& raw, size_t& index) {
    skipSpace(raw, index);
    if (index >= raw.size())
        return false;
    char ch = raw[index];
    if (ch == '"') {
        std::string ignored;
        return parseString(raw, index, ignored);
    }
    if (ch == '{' || ch == '[')
        return skipContainer(raw, index);
    if (ch == 't' || ch == 'f' || ch == 'n') {
        while (index < raw.size() && isalpha((unsigned char)raw[index]))
            index++;
        return true;
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        while (index < raw.size() && (isdigit((unsigned char)raw[index]) || raw[index] == '.' || raw[index] == 'e' || raw[index] == 'E' || raw[index] == '+' || raw[index] == '-'))
            index++;
        return true;
    }
    return false;
}

inline bool findKey(const std::string& raw, const char* key, size_t& index) {
    index = 0;
    skipSpace(raw, index);
    if (index < raw.size() && raw[index] == '{')
        index++;

    while (index < raw.size()) {
        skipSpace(raw, index);
        if (index >= raw.size())
            return false;
        if (raw[index] == '}')
            return false;
        std::string name;
        if (!parseString(raw, index, name))
            return false;
        skipSpace(raw, index);
        if (index >= raw.size() || raw[index] != ':')
            return false;
        index++;
        if (name == key)
            return true;
        if (!skipValue(raw, index))
            return false;
        skipSpace(raw, index);
        if (index < raw.size() && raw[index] == ',')
            index++;
    }
    return false;
}

inline std::string getString(const std::string& raw, const char* key) {
    size_t index = 0;
    if (!findKey(raw, key, index))
        return "";
    std::string value;
    if (!parseString(raw, index, value))
        return "";
    return value;
}

inline int getInt(const std::string& raw, const char* key, int fallback = 0) {
    size_t index = 0;
    if (!findKey(raw, key, index))
        return fallback;
    skipSpace(raw, index);
    if (index >= raw.size())
        return fallback;
    bool negative = raw[index] == '-';
    if (negative)
        index++;
    if (index >= raw.size() || !isdigit((unsigned char)raw[index]))
        return fallback;
    int value = 0;
    while (index < raw.size() && isdigit((unsigned char)raw[index]))
        value = value * 10 + (raw[index++] - '0');
    return negative ? -value : value;
}

inline bool getBool(const std::string& raw, const char* key, bool fallback = false) {
    size_t index = 0;
    if (!findKey(raw, key, index))
        return fallback;
    skipSpace(raw, index);
    if (raw.compare(index, 4, "true") == 0)
        return true;
    if (raw.compare(index, 5, "false") == 0)
        return false;
    return fallback;
}

inline std::vector<std::string> parseObjectArrayAt(const std::string& raw, size_t index) {
    std::vector<std::string> objects;
    skipSpace(raw, index);
    if (index >= raw.size() || raw[index] != '[')
        return objects;
    index++;
    while (index < raw.size()) {
        skipSpace(raw, index);
        if (index >= raw.size())
            break;
        if (raw[index] == ']')
            break;
        if (raw[index] != '{') {
            if (!skipValue(raw, index))
                break;
            skipSpace(raw, index);
            if (index < raw.size() && raw[index] == ',')
                index++;
            continue;
        }
        size_t start = index;
        if (!skipContainer(raw, index))
            break;
        objects.push_back(raw.substr(start, index - start));
        skipSpace(raw, index);
        if (index < raw.size() && raw[index] == ',')
            index++;
    }
    return objects;
}

inline std::vector<std::string> parseObjectArray(const std::string& raw) {
    return parseObjectArrayAt(raw, 0);
}

inline std::vector<std::string> getObjectArray(const std::string& raw, const char* key) {
    size_t index = 0;
    if (!findKey(raw, key, index))
        return std::vector<std::string>();
    return parseObjectArrayAt(raw, index);
}

inline std::string escape(const std::string& source) {
    std::string out;
    out.reserve(source.size() + 8);
    for (unsigned char ch : source) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out.push_back((char)ch);
                }
                break;
        }
    }
    return out;
}

}
