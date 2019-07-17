#include "pch.h"
#include "rthsLog.h"

namespace rths {

static std::string g_error_log;

const std::string& GetErrorLog()
{
    return g_error_log;
}

void SetErrorLog(const char *format, ...)
{
    const int MaxBuf = 2048;
    char buf[MaxBuf];

    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    g_error_log = buf;
    va_end(args);
}

void SetErrorLog(const std::string& str)
{
    g_error_log = str;
}

void DebugPrintImpl(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    ::OutputDebugStringA(buf);
#else
    vprintf(fmt, args);
#endif
    va_end(args);
}

} // namespace rths
