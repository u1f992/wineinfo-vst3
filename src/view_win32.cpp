#include "view_win32.h"
#include <cstring>
#include <windowsx.h>

using namespace Steinberg;

namespace WineInfo {

ViewWin32::ViewWin32(std::wstring text) : text_(std::move(text)) {}
ViewWin32::~ViewWin32() { removed(); }

tresult PLUGIN_API ViewWin32::queryInterface(const TUID _iid, void **obj) {
  QUERY_INTERFACE(_iid, obj, FUnknown::iid, IPlugView)
  QUERY_INTERFACE(_iid, obj, IPlugView::iid, IPlugView)
  *obj = nullptr;
  return kNoInterface;
}

tresult PLUGIN_API ViewWin32::isPlatformTypeSupported(FIDString type) {
  return (type && std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue
                                                             : kResultFalse;
}

static ATOM ensure_class() {
  static ATOM a = 0;
  if (a)
    return a;

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = ViewWin32::WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"WineInfoVST3View";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  a = RegisterClassExW(&wc);
  return a;
}

tresult PLUGIN_API ViewWin32::attached(void *parent, FIDString type) {
  if (isPlatformTypeSupported(type) != kResultTrue)
    return kResultFalse;
  if (!parent)
    return kResultFalse;

  ensure_class();

  HWND host = (HWND)parent;
  hwnd_ = CreateWindowExW(0, L"WineInfoVST3View", L"", WS_CHILD | WS_VISIBLE, 0,
                          0, rect_.right - rect_.left, rect_.bottom - rect_.top,
                          host, nullptr, GetModuleHandleW(nullptr), this);
  return hwnd_ ? kResultOk : kResultFalse;
}

tresult PLUGIN_API ViewWin32::removed() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  return kResultOk;
}

tresult PLUGIN_API ViewWin32::getSize(ViewRect *size) {
  if (!size)
    return kResultFalse;
  *size = rect_;
  return kResultOk;
}

tresult PLUGIN_API ViewWin32::onSize(ViewRect *newSize) {
  if (!newSize)
    return kResultFalse;
  rect_ = *newSize;
  if (hwnd_) {
    SetWindowPos(hwnd_, nullptr, 0, 0, rect_.right - rect_.left,
                 rect_.bottom - rect_.top, SWP_NOZORDER | SWP_NOMOVE);
    InvalidateRect(hwnd_, nullptr, TRUE);
  }
  return kResultOk;
}

LRESULT CALLBACK ViewWin32::WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  ViewWin32 *self = (ViewWin32 *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

  if (msg == WM_NCCREATE) {
    auto *cs = (CREATESTRUCTW *)l;
    self = (ViewWin32 *)cs->lpCreateParams;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    return DefWindowProcW(hwnd, msg, w, l);
  }

  switch (msg) {
  case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT r{};
    GetClientRect(hwnd, &r);

    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, self ? self->text_.c_str() : L"", -1, &r,
              DT_LEFT | DT_TOP | DT_WORDBREAK);
    EndPaint(hwnd, &ps);
    return 0;
  }
  default:
    return DefWindowProcW(hwnd, msg, w, l);
  }
}

} // namespace WineInfo
