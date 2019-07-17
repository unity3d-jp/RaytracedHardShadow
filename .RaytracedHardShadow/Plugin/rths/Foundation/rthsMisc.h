#pragma once

namespace rths {

using nanosec = uint64_t;
nanosec Now();
inline float NS2MS(nanosec ns) { return float((double)ns / 1000000.0); }

std::string ToUTF8(const char *src);
std::string ToUTF8(const std::string& src);
std::string ToANSI(const char *src);
std::string ToANSI(const std::string& src);
std::string ToMBS(const wchar_t *src);
std::string ToMBS(const std::wstring& src);
std::wstring ToWCS(const char *src);
std::wstring ToWCS(const std::string& src);

} // namespace rths
