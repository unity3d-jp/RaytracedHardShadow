#include "pch.h"
#include "rthsMisc.h"

namespace rths {

nanosec Now()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string ToUTF8(const char *src)
{
#ifdef _WIN32
    // to UTF-16
    int wsize = ::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)src, -1, nullptr, 0);
    if (wsize > 0)
        --wsize; // remove last '\0'
    std::wstring ws;
    ws.resize(wsize);
    ::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)src, -1, (LPWSTR)ws.data(), wsize);

    // to UTF-8
    int u8size = ::WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)ws.data(), -1, nullptr, 0, nullptr, nullptr);
    if (u8size > 0)
        --u8size; // remove last '\0'
    std::string u8s;
    u8s.resize(u8size);
    ::WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)ws.data(), -1, (LPSTR)u8s.data(), u8size, nullptr, nullptr);
    return u8s;
#else
    return src;
#endif
}
std::string ToUTF8(const std::string& src)
{
    return ToUTF8(src.c_str());
}

std::string ToANSI(const char *src)
{
#ifdef _WIN32
    // to UTF-16
    int wsize = ::MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)src, -1, nullptr, 0);
    if (wsize > 0)
        --wsize; // remove last '\0'
    std::wstring ws;
    ws.resize(wsize);
    ::MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)src, -1, (LPWSTR)ws.data(), wsize);

    // to ANSI
    int u8size = ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)ws.data(), -1, nullptr, 0, nullptr, nullptr);
    if (u8size > 0)
        --u8size; // remove last '\0'
    std::string u8s;
    u8s.resize(u8size);
    ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)ws.data(), -1, (LPSTR)u8s.data(), u8size, nullptr, nullptr);
    return u8s;
#else
    return src;
#endif
}
std::string ToANSI(const std::string& src)
{
    return ToANSI(src.c_str());
}

std::string ToMBS(const wchar_t * src)
{
    using converter_t = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>;
    return converter_t().to_bytes(src);
}
std::string ToMBS(const std::wstring& src)
{
    return ToMBS(src.c_str());
}

std::wstring ToWCS(const char * src)
{
    using converter_t = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>;
    return converter_t().from_bytes(src);
}
std::wstring ToWCS(const std::string & src)
{
    return ToWCS(src.c_str());
}

} // namespace rths
