#include "view_d3d11.h"
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <utility>

using namespace Steinberg;
using Microsoft::WRL::ComPtr;

namespace WineInfo {

static constexpr wchar_t kWindowClassName[] = L"WineInfoVST3D3D11View";
static const ViewRect kInitialRect{0, 0, 560, 150};

static HINSTANCE thisModule() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&thisModule), &module);
  return module;
}

static D2D1_COLOR_F systemColor(int index) {
  COLORREF color = GetSysColor(index);
  return D2D1::ColorF(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f,
                      GetBValue(color) / 255.0f);
}

ViewD3D11::ViewD3D11(InfoStrings info)
    : CPluginView(&kInitialRect), info_(std::move(info)) {}

tresult PLUGIN_API ViewD3D11::isPlatformTypeSupported(FIDString type) {
  return (type && std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue
                                                             : kResultFalse;
}

tresult PLUGIN_API ViewD3D11::attached(void *parent, FIDString type) {
  if (!parent || isPlatformTypeSupported(type) != kResultTrue)
    return kResultFalse;

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = wndProc;
  wc.hInstance = thisModule();
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      0, kWindowClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, rect.getWidth(),
      rect.getHeight(), static_cast<HWND>(parent), nullptr, thisModule(), this);
  if (!hwnd_)
    return kResultFalse;
  if (!createGraphics()) {
    removed();
    return kResultFalse;
  }
  return CPluginView::attached(parent, type);
}

tresult PLUGIN_API ViewD3D11::removed() {
  releaseGraphics();
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

tresult PLUGIN_API ViewD3D11::onSize(ViewRect *newSize) {
  tresult result = CPluginView::onSize(newSize);
  if (result != kResultTrue || !hwnd_)
    return result;

  SetWindowPos(hwnd_, nullptr, 0, 0, rect.getWidth(), rect.getHeight(),
               SWP_NOZORDER | SWP_NOMOVE);
  if (swapChain_) {
    renderTarget_.Reset();
    swapChain_->ResizeBuffers(0, rect.getWidth(), rect.getHeight(),
                              DXGI_FORMAT_UNKNOWN, 0);
    createRenderTarget();
  }
  return result;
}

bool ViewD3D11::createGraphics() {
  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 1;
  desc.OutputWindow = hwnd_;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  HRESULT hr = E_FAIL;
  for (D3D_DRIVER_TYPE driverType :
       {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr, driverType, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
        0, D3D11_SDK_VERSION, &desc, &swapChain_, &device_, nullptr, nullptr);
    if (SUCCEEDED(hr))
      break;
  }
  if (FAILED(hr))
    return false;
  swapChainPresented_ = false;

  if (FAILED(D2D1CreateFactory(
          D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr,
          reinterpret_cast<void **>(d2dFactory_.ReleaseAndGetAddressOf()))))
    return false;
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                 __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown **>(
                                     dwriteFactory_.ReleaseAndGetAddressOf()))))
    return false;

  if (FAILED(dwriteFactory_->CreateTextFormat(
          L"Tahoma", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
          DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"",
          textFormat_.ReleaseAndGetAddressOf())))
    return false;
  textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  ComPtr<IDWriteInlineObject> ellipsis;
  dwriteFactory_->CreateEllipsisTrimmingSign(textFormat_.Get(), &ellipsis);
  DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
  textFormat_->SetTrimming(&trimming, ellipsis.Get());

  return createRenderTarget();
}

bool ViewD3D11::createRenderTarget() {
  ComPtr<IDXGISurface> backBuffer;
  if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
    return false;

  D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED),
      96.0f, 96.0f);
  return SUCCEEDED(d2dFactory_->CreateDxgiSurfaceRenderTarget(
      backBuffer.Get(), &properties, renderTarget_.ReleaseAndGetAddressOf()));
}

void ViewD3D11::releaseGraphics() {
  renderTarget_.Reset();
  textFormat_.Reset();
  dwriteFactory_.Reset();
  d2dFactory_.Reset();
  swapChain_.Reset();
  device_.Reset();
}

LRESULT CALLBACK ViewD3D11::wndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  if (msg == WM_NCCREATE) {
    auto *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }

  auto *self =
      reinterpret_cast<ViewD3D11 *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_PAINT && self) {
    ValidateRect(hwnd, nullptr);
    self->render();
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ViewD3D11::render() {
  if (!renderTarget_)
    return;

  const std::pair<const wchar_t *, const std::wstring *> rows[] = {
      {L"Wine version", &info_.version},
      {L"Wine build-id", &info_.build_id},
      {L"/proc/self/exe", &info_.proc_self_exe},
      {L"WINELOADER", &info_.wineloader},
      {L"WINEPREFIX", &info_.wineprefix},
      {L"WINESERVER", &info_.wineserver},
  };

  float labelWidth = 0.0f;
  for (const auto &[label, value] : rows) {
    ComPtr<IDWriteTextLayout> layout;
    DWRITE_TEXT_METRICS metrics{};
    if (SUCCEEDED(dwriteFactory_->CreateTextLayout(label, lstrlenW(label),
                                                   textFormat_.Get(), FLT_MAX,
                                                   FLT_MAX, &layout)) &&
        SUCCEEDED(layout->GetMetrics(&metrics)))
      labelWidth =
          std::max(labelWidth, metrics.widthIncludingTrailingWhitespace);
  }

  ComPtr<ID2D1SolidColorBrush> labelBrush;
  ComPtr<ID2D1SolidColorBrush> valueBrush;
  renderTarget_->CreateSolidColorBrush(systemColor(COLOR_GRAYTEXT),
                                       &labelBrush);
  renderTarget_->CreateSolidColorBrush(systemColor(COLOR_WINDOWTEXT),
                                       &valueBrush);

  const D2D1_SIZE_F size = renderTarget_->GetSize();
  constexpr float margin = 12.0f;
  constexpr float rowHeight = 19.0f;

  renderTarget_->BeginDraw();
  renderTarget_->Clear(systemColor(COLOR_WINDOW));
  float y = margin;
  for (const auto &[label, value] : rows) {
    renderTarget_->DrawText(
        label, lstrlenW(label), textFormat_.Get(),
        D2D1::RectF(margin, y, margin + labelWidth, y + rowHeight),
        labelBrush.Get());
    renderTarget_->DrawText(value->c_str(), static_cast<UINT32>(value->size()),
                            textFormat_.Get(),
                            D2D1::RectF(margin + labelWidth + margin, y,
                                        size.width - margin, y + rowHeight),
                            valueBrush.Get());
    y += rowHeight;
  }

  if (renderTarget_->EndDraw() == D2DERR_RECREATE_TARGET) {
    releaseGraphics();
    if (createGraphics())
      InvalidateRect(hwnd_, nullptr, FALSE);
    return;
  }
  swapChain_->Present(1, 0);

  // Under Wine, the first Present of a child window's swap chain reaches an X
  // drawable that is not yet sized to the window, so only a single pixel of
  // that frame appears. Frames presented after it cover the whole window.
  if (!swapChainPresented_) {
    swapChainPresented_ = true;
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

} // namespace WineInfo
