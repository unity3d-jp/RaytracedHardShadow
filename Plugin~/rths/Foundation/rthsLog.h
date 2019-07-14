#pragma once

namespace rths {

std::string GetErrorLog();
void SetErrorLog(const char *format, ...);
void SetErrorLog(const std::string& str);
void ClearErrorLog();
void DebugPrintImpl(const char *fmt, ...);

#ifdef rthsDebug
    #define DebugPrint(...) DebugPrintImpl(__VA_ARGS__)
#else
    #define DebugPrint(...)
#endif

} // namespace rths
