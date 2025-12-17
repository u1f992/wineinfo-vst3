#pragma once
#include <string>

namespace WineInfo {

struct InfoStrings {
  std::wstring version;
  std::wstring build_id;
  std::wstring proc_self_exe;
  std::wstring wineloader;
  std::wstring wineprefix;
  std::wstring wineserver;
};

InfoStrings get_info_strings();

} // namespace WineInfo
