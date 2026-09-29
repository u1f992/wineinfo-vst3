#pragma once
#include "public.sdk/source/common/pluginview.h"
#include "wineinfo.h"
#include <windows.h>

namespace WineInfo {

class ViewWin32 final : public Steinberg::CPluginView {
public:
  explicit ViewWin32(InfoStrings info);

  Steinberg::tresult PLUGIN_API
  isPlatformTypeSupported(Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API attached(void *parent,
                                         Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API removed() override;
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect *newSize) override;

private:
  static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
  void paint(HDC hdc) const;

  InfoStrings info_;
  HWND hwnd_ = nullptr;
};

} // namespace WineInfo
