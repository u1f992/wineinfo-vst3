#pragma once
#include "public.sdk/source/common/pluginview.h"
#include "wineinfo.h"
#include <memory>
#include <windows.h>

namespace WineInfo {

class VulkanPresenter;

class ViewVulkan final : public Steinberg::CPluginView {
public:
  explicit ViewVulkan(InfoStrings info);
  ~ViewVulkan() override;

  Steinberg::tresult PLUGIN_API
  isPlatformTypeSupported(Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API attached(void *parent,
                                         Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API removed() override;
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect *newSize) override;

private:
  static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
  void render();
  void paint(HDC hdc, const RECT &client) const;

  InfoStrings info_;
  HWND hwnd_ = nullptr;
  std::unique_ptr<VulkanPresenter> presenter_;
};

} // namespace WineInfo
