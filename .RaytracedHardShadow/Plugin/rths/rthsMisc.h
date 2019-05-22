#pragma once

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

namespace rths {

std::string ToUTF8(const char *src);
std::string ToUTF8(const std::string& src);
std::string ToANSI(const char *src);
std::string ToANSI(const std::string& src);
std::string ToMBS(const wchar_t *src);
std::string ToMBS(const std::wstring& src);
std::wstring ToWCS(const char *src);
std::wstring ToWCS(const std::string& src);

} // namespace rths
