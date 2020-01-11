#pragma once

#include <string>
#include <wchar.h>
#include <utf8/utf8.h>
#include <algorithm>
#include <memory>

#ifdef WIN32
#include <wcwidth.h>
#endif

inline std::wstring u8to16(const std::string& u8) {
    std::wstring result;
    utf8::utf8to16(u8.begin(), u8.end(), std::back_inserter(result));
    return result;
}

inline std::string u16to8(const std::wstring& u16) {
    std::string result;
    utf8::utf16to8(u16.begin(), u16.end(), std::back_inserter(result));
    return result;
}

static inline size_t u8cols(const std::string& str) {
    std::wstring wstr = u8to16(str);
#ifdef WIN32
    int result = std::max(0, mk_wcswidth(wstr.c_str(), wstr.size()));
#else
    int result = std::max(0, wcswidth(wstr.c_str(), wstr.size()));
#endif
    return (result > 0) ? result : str.size();
}

inline static size_t u8len(const std::string& str) {
    try {
        return utf8::distance(str.begin(), str.end());
    }
    catch (...) {
        return str.length();
    }
}

/* get the (raw) character index of the "nth" logical/display character */
inline static size_t u8offset(const std::string& str, int n) {
    if (str.size() == 0) {
        return std::string::npos;
    }

    std::string::const_iterator it = str.begin();

    int count = 0;
    while (count < n && it != str.end()) {
        utf8::unchecked::next(it);
        ++count;
    }

    return (size_t)(it - str.begin());
}

inline static std::string u8substr(const std::string& in, int offset, int len) {
    std::string::const_iterator begin = in.begin() + offset;
    std::string::const_iterator it = begin;

    int count = 0;
    while (count < len && it != in.end()) {
        utf8::unchecked::next(it);
        ++count;
    }

    return std::string(begin, it);
}

template<typename... Args>
static std::string u8fmt(const std::string& format, Args ... args) {
    /* https://stackoverflow.com/a/26221725 */
    size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; /* extra space for '\0' */
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); /* omit the '\0' */
}

static inline void u8replace(
    std::string& input, const std::string& find, const std::string& replace)
{
    size_t pos = input.find(find);
    while (pos != std::string::npos) {
        input.replace(pos, find.size(), replace);
        pos = input.find(find, pos + replace.size());
    }
}
