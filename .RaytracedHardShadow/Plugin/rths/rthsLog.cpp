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

} // namespace rths
