#include "wineinfo.h"
#include <string>
#include <vector>
#include <windows.h>

static std::wstring widen_utf8(const std::string &s) {
  if (s.empty())
    return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  if (n <= 0)
    return L"";
  std::vector<wchar_t> buf(n);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, buf.data(), n);
  return std::wstring(buf.data());
}

static std::string get_proc_string(HMODULE mod, const char *name) {
  if (!mod)
    return "";
  auto p = (const char *(*)())GetProcAddress(mod, name);
  if (!p)
    return "";
  const char *s = p();
  return s ? std::string(s) : std::string("");
}

static std::wstring getenv_w(const wchar_t *key) {
  DWORD n = GetEnvironmentVariableW(key, nullptr, 0);
  if (n == 0)
    return L"";
  std::vector<wchar_t> buf(n);
  DWORD m = GetEnvironmentVariableW(key, buf.data(), n);
  if (m == 0)
    return L"";
  return std::wstring(buf.data());
}

static std::wstring best_effort_proc_self_exe() {
  HANDLE h =
      CreateFileW(L"Z:\\proc\\self\\exe", 0,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return L"";

  std::vector<wchar_t> buf(4096);
  DWORD len = GetFinalPathNameByHandleW(h, buf.data(), (DWORD)buf.size(),
                                        FILE_NAME_NORMALIZED);
  CloseHandle(h);
  if (len == 0 || len >= buf.size())
    return L"";
  return std::wstring(buf.data());
}

namespace WineInfo {

InfoStrings get_info_strings() {
  HMODULE ntdll = GetModuleHandleA("ntdll.dll");

  std::string ver = get_proc_string(ntdll, "wine_get_version");
  std::string bid = get_proc_string(ntdll, "wine_get_build_id");

  InfoStrings info;
  info.version = widen_utf8(ver);
  info.build_id = widen_utf8(bid);
  info.wineloader = getenv_w(L"WINELOADER");
  info.wineprefix = getenv_w(L"WINEPREFIX");
  info.wineserver = getenv_w(L"WINESERVER");
  info.proc_self_exe = best_effort_proc_self_exe();

  auto na = L"(N/A)";
  if (info.version.empty())
    info.version = L"(Not Wine or unknown)";
  if (info.build_id.empty())
    info.build_id = na;
  if (info.wineloader.empty())
    info.wineloader = na;
  if (info.wineprefix.empty())
    info.wineprefix = na;
  if (info.wineserver.empty())
    info.wineserver = na;
  if (info.proc_self_exe.empty())
    info.proc_self_exe = na;

  return info;
}

} // namespace WineInfo
