/*
    LogFormat.cpp - Renders one logged Storm.dll call into a line of text, according
    to the user-configurable log format template. See LogFormat.h.
*/

#include "LogFormat.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstdio>

std::string PointerToString(const void* ptr)
{
    if (!ptr)
        return "(null)";
    std::ostringstream oss;
    oss << ptr;
    return oss.str();
}

std::string PointerOnly(const char* name, const void* ptr)
{
    return std::string(name) + "=" + PointerToString(ptr);
}

std::string PointerWithDword(const char* name, const DWORD* ptr)
{
    std::string s = PointerOnly(name, ptr);
    if (ptr)
        s += " (deref=" + std::to_string(*ptr) + ")";
    return s;
}

std::string PointerWithHandle(const char* name, HANDLE* ptr)
{
    std::string s = PointerOnly(name, ptr);
    if (ptr && *ptr)
        s += " (deref=" + PointerToString(*ptr) + ")";
    return s;
}

std::string PointerWithPointer(const char* name, void* const* ptr)
{
    std::string s = PointerOnly(name, ptr);
    if (ptr)
        s += " (deref=" + PointerToString(*ptr) + ")";
    return s;
}

// Renders the ISO-8601 UTC form of a timestamp, e.g. "2026-08-27T14:03:21.123Z"
static std::string FormatIso8601(const struct tm& utc, int millis)
{
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << millis << 'Z';
    return oss.str();
}

// Finds the index of the first unescaped '}' at or after `start` in `s`, treating
// an escaped "%}" pair as literal content rather than the sub-format's terminator.
// Returns std::string::npos if no unescaped '}' is found before the end of the string.
static size_t FindSubFormatEnd(const std::string& s, size_t start)
{
    for (size_t i = start; i < s.size(); ++i)
    {
        if (s[i] == '%' && i + 1 < s.size() && s[i + 1] == '}')
        {
            ++i; // skip the escaped pair - the loop's ++i moves past the '}' too
            continue;
        }
        if (s[i] == '}')
            return i;
    }
    return std::string::npos;
}

// Expands our own tokens inside a %t{...} sub-format before handing the result to
// strftime: %L becomes the zero-padded millisecond value, and %} becomes a literal
// '}' (needed since an escaped "%}" reaches here still in its escaped form - the
// terminator scan above only used it to know the span didn't end there). %% and
// every real strftime specifier are left completely untouched for strftime's own
// pass - this function never introduces a '%' character of its own, so it can't
// create a sequence strftime would misinterpret.
static std::string ExpandTimestampTokens(const std::string& subFormat, int millis)
{
    std::string result;
    result.reserve(subFormat.size());

    size_t i = 0;
    while (i < subFormat.size())
    {
        if (subFormat[i] == '%' && i + 1 < subFormat.size())
        {
            char next = subFormat[i + 1];
            if (next == 'L')
            {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%03d", millis);
                result += buf;
                i += 2;
                continue;
            }
            if (next == '}')
            {
                result += '}';
                i += 2;
                continue;
            }
        }
        result += subFormat[i];
        ++i;
    }

    return result;
}

// Renders a user-authored %t{...} sub-format via strftime, after expanding our own
// %L (milliseconds) and %} (literal brace) tokens. This CRT's strftime returns 0 for
// the *entire* call the moment the format contains any specifier it doesn't
// recognize or support - not just that one token - so a typo or an unsupported
// specifier (e.g. %T, which this CRT doesn't implement) would otherwise produce a
// silently blank timestamp. Instead, on failure the original, unresolved "%t{...}"
// text is returned, so a misconfigured format stays visible in the log rather than
// disappearing.
static std::string FormatCustomTimestamp(const std::string& subFormat, const struct tm& utc, int millis)
{
    std::string expanded = ExpandTimestampTokens(subFormat, millis);
    if (expanded.empty())
        return "";

    char buf[256];
    size_t n = strftime(buf, sizeof(buf), expanded.c_str(), &utc);
    if (n == 0)
        return "%t{" + subFormat + "}";

    return std::string(buf, n);
}

std::string FormatLogEntry(const std::string& format, const std::string& archiveName,
                            const std::string& fileName, const std::string& callName,
                            const std::string& nonPointerParams, const std::string& pointerParams)
{
    // Computed once so every timestamp placeholder in this line reflects the same instant
    auto now = std::chrono::system_clock::now();
    auto msSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    time_t seconds = static_cast<time_t>(msSinceEpoch / 1000);
    int millis = static_cast<int>(msSinceEpoch % 1000);
    struct tm utc{};
    gmtime_s(&utc, &seconds);

    std::string result;
    result.reserve(format.size());

    size_t i = 0;
    while (i < format.size())
    {
        char c = format[i];
        if (c != '%' || i + 1 >= format.size())
        {
            result += c;
            ++i;
            continue;
        }

        char specifier = format[i + 1];

        // %t{...} - custom strftime-based timestamp sub-format
        if (specifier == 't' && i + 2 < format.size() && format[i + 2] == '{')
        {
            size_t contentStart = i + 3;
            size_t closePos = FindSubFormatEnd(format, contentStart);
            if (closePos != std::string::npos)
            {
                std::string subFormat = format.substr(contentStart, closePos - contentStart);
                result += FormatCustomTimestamp(subFormat, utc, millis);
                i = closePos + 1;
                continue;
            }
            // No closing '}' found: fall through and treat this as a bare %t: the
            // stray '{' is then just an ordinary character on the next iteration.
        }

        switch (specifier)
        {
            case 't': result += std::to_string(msSinceEpoch); break;
            case 'T': result += FormatIso8601(utc, millis);   break;
            case 'a': result += archiveName;                  break;
            case 'f': result += fileName;                     break;
            case 'c': result += callName;                     break;
            case 'p': result += nonPointerParams;             break;
            case 'P': result += pointerParams;                break;
            case '{': result += '{';                          break;
            case '}': result += '}';                          break;
            case '%': result += '%';                          break;
            default:
                // Unrecognized specifier: keep both characters literally
                result += '%';
                result += specifier;
                break;
        }
        i += 2;
    }

    return result;
}
