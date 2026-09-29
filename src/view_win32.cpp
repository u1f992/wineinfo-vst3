#include "view_win32.h"
#include <algorithm>
#include <cstring>
#include <utility>

using namespace Steinberg;

namespace WineInfo {

static constexpr wchar_t kWindowClassName[] = L"WineInfoVST3View";
static const ViewRect kInitialRect{0, 0, 560, 150};

static HINSTANCE thisModule() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&thisModule), &module);
  return module;
}

ViewWin32::ViewWin32(InfoStrings info)
    : CPluginView(&kInitialRect), info_(std::move(info)) {}

tresult PLUGIN_API ViewWin32::isPlatformTypeSupported(FIDString type) {
  return (type && std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue
                                                             : kResultFalse;
}

tresult PLUGIN_API ViewWin32::attached(void *parent, FIDString type) {
  if (!parent || isPlatformTypeSupported(type) != kResultTrue)
    return kResultFalse;

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = wndProc;
  wc.hInstance = thisModule();
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      0, kWindowClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, rect.getWidth(),
      rect.getHeight(), static_cast<HWND>(parent), nullptr, thisModule(), this);
  if (!hwnd_)
    return kResultFalse;
  return CPluginView::attached(parent, type);
}

tresult PLUGIN_API ViewWin32::removed() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  // Classes registered by a DLL outlive its unloading, which would leave the
  // class pointing at a stale wndProc. This fails harmlessly while another
  // instance still has a window of this class.
  UnregisterClassW(kWindowClassName, thisModule());
  return CPluginView::removed();
}

tresult PLUGIN_API ViewWin32::onSize(ViewRect *newSize) {
  tresult result = CPluginView::onSize(newSize);
  if (result == kResultTrue && hwnd_)
    SetWindowPos(hwnd_, nullptr, 0, 0, rect.getWidth(), rect.getHeight(),
                 SWP_NOZORDER | SWP_NOMOVE);
  return result;
}

LRESULT CALLBACK ViewWin32::wndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  if (msg == WM_NCCREATE) {
    auto *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }

  auto *self =
      reinterpret_cast<ViewWin32 *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_PAINT && self) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    self->paint(hdc);
    EndPaint(hwnd, &ps);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ViewWin32::paint(HDC hdc) const {
  const std::pair<const wchar_t *, const std::wstring *> rows[] = {
      {L"Wine version", &info_.version},
      {L"Wine build-id", &info_.build_id},
      {L"/proc/self/exe", &info_.proc_self_exe},
      {L"WINELOADER", &info_.wineloader},
      {L"WINEPREFIX", &info_.wineprefix},
      {L"WINESERVER", &info_.wineserver},
  };

  HGDIOBJ previousFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hdc, TRANSPARENT);

  TEXTMETRICW tm{};
  GetTextMetricsW(hdc, &tm);
  LONG labelWidth = 0;
  for (const auto &[label, value] : rows) {
    SIZE extent{};
    GetTextExtentPoint32W(hdc, label, lstrlenW(label), &extent);
    labelWidth = std::max(labelWidth, extent.cx);
  }

  RECT client{};
  GetClientRect(hwnd_, &client);
  constexpr LONG margin = 12;
  const LONG rowHeight = tm.tmHeight + tm.tmExternalLeading + 6;
  const UINT format = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;

  LONG y = margin;
  for (const auto &[label, value] : rows) {
    RECT labelRect{margin, y, margin + labelWidth, y + rowHeight};
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawTextW(hdc, label, -1, &labelRect, format);

    RECT valueRect{labelRect.right + margin, y, client.right - margin,
                   y + rowHeight};
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    DrawTextW(hdc, value->c_str(), -1, &valueRect, format | DT_PATH_ELLIPSIS);

    y += rowHeight;
  }

  SelectObject(hdc, previousFont);
}

} // namespace WineInfo
