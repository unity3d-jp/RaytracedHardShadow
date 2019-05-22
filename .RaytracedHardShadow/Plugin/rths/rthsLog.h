#pragma once

namespace rths {

const std::string& GetErrorLog();
void SetErrorLog(const char *format, ...);
void SetErrorLog(const std::string& str);

} // namespace rths
