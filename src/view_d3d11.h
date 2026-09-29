#pragma once
#include "public.sdk/source/common/pluginview.h"
#include "wineinfo.h"
#include <d2d1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

namespace WineInfo {

class ViewD3D11 final : public Steinberg::CPluginView {
public:
  explicit ViewD3D11(InfoStrings info);

  Steinberg::tresult PLUGIN_API
  isPlatformTypeSupported(Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API attached(void *parent,
                                         Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API removed() override;
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect *newSize) override;

private:
  static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
  bool createGraphics();
  bool createRenderTarget();
  void releaseGraphics();
  void render();

  InfoStrings info_;
  HWND hwnd_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
  bool swapChainPresented_ = false;
  Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
  Microsoft::WRL::ComPtr<ID2D1RenderTarget> renderTarget_;
  Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
};

} // namespace WineInfo
