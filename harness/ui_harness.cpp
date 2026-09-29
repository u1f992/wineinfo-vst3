#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include <windows.h>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr wchar_t kFrameClassName[] = L"WineInfoUIHarnessFrame";

struct Options {
  std::string pluginPath;
  std::string screenshotPath;
  bool interactive = false;
};

bool check(bool condition, const char *what) {
  std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", what);
  return condition;
}

class PlugFrame : public U::Implements<U::Directly<IPlugFrame>> {
public:
  explicit PlugFrame(HWND window) : window_(window) {}

  tresult PLUGIN_API resizeView(IPlugView *view, ViewRect *newSize) override {
    if (!view || !newSize)
      return kInvalidArgument;
    resizeFrameToClientSize(window_, newSize->getWidth(), newSize->getHeight());
    return view->onSize(newSize);
  }

  static void resizeFrameToClientSize(HWND window, int width, int height) {
    RECT r{0, 0, width, height};
    AdjustWindowRectEx(&r, GetWindowLongW(window, GWL_STYLE), FALSE,
                       GetWindowLongW(window, GWL_EXSTYLE));
    SetWindowPos(window, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOMOVE);
  }

private:
  HWND window_;
};

LRESULT CALLBACK frameWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                              LPARAM lParam) {
  if (msg == WM_CLOSE) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND createFrameWindow(const ViewRect &size) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = frameWndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kFrameClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  HWND frame = CreateWindowExW(
      0, kFrameClassName, L"WineInfo UI harness",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
      GetModuleHandleW(nullptr), nullptr);
  if (frame) {
    PlugFrame::resizeFrameToClientSize(frame, size.getWidth(),
                                       size.getHeight());
    ShowWindow(frame, SW_SHOW);
  }
  return frame;
}

void pumpPendingMessages() {
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

void runMessageLoop() {
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

SIZE clientSize(HWND window) {
  RECT r{};
  GetClientRect(window, &r);
  return {r.right - r.left, r.bottom - r.top};
}

std::vector<DWORD> capturePixels(HWND window, SIZE size) {
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = size.cx;
  bi.bmiHeader.biHeight = -size.cy;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  // Wine 10 copies Vulkan presentations of child windows straight to the X
  // window of the top-level window, bypassing the child's DC, so read back
  // what is on screen instead.
  POINT origin{0, 0};
  ClientToScreen(window, &origin);
  HDC screenDC = GetDC(nullptr);
  HDC memoryDC = CreateCompatibleDC(screenDC);
  void *bits = nullptr;
  HBITMAP bitmap =
      CreateDIBSection(screenDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ previous = SelectObject(memoryDC, bitmap);
  BitBlt(memoryDC, 0, 0, size.cx, size.cy, screenDC, origin.x, origin.y,
         SRCCOPY);
  SelectObject(memoryDC, previous);

  const auto *first = static_cast<const DWORD *>(bits);
  std::vector<DWORD> pixels(first, first + size.cx * size.cy);

  DeleteObject(bitmap);
  DeleteDC(memoryDC);
  ReleaseDC(nullptr, screenDC);
  return pixels;
}

bool writeBmp(const std::string &path, SIZE size,
              const std::vector<DWORD> &pixels) {
  BITMAPINFOHEADER info{};
  info.biSize = sizeof(info);
  info.biWidth = size.cx;
  info.biHeight = -size.cy;
  info.biPlanes = 1;
  info.biBitCount = 32;
  info.biCompression = BI_RGB;

  const DWORD pixelBytes = static_cast<DWORD>(pixels.size() * sizeof(DWORD));
  BITMAPFILEHEADER file{};
  file.bfType = 0x4D42;
  file.bfOffBits = sizeof(file) + sizeof(info);
  file.bfSize = file.bfOffBits + pixelBytes;

  FILE *out = std::fopen(path.c_str(), "wb");
  if (!out)
    return false;
  bool ok = std::fwrite(&file, sizeof(file), 1, out) == 1 &&
            std::fwrite(&info, sizeof(info), 1, out) == 1 &&
            std::fwrite(pixels.data(), pixelBytes, 1, out) == 1;
  return std::fclose(out) == 0 && ok;
}

bool parseOptions(int argc, char **argv, Options &options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--interactive")
      options.interactive = true;
    else if (arg == "--screenshot" && i + 1 < argc)
      options.screenshotPath = argv[++i];
    else if (options.pluginPath.empty() && arg.rfind("--", 0) != 0)
      options.pluginPath = arg;
    else
      return false;
  }
  return !options.pluginPath.empty();
}

IPtr<PlugProvider> loadPlugin(const VST3::Hosting::Module::Ptr &module) {
  for (auto &classInfo : module->getFactory().classInfos()) {
    if (classInfo.category() != kVstAudioEffectClass)
      continue;
    auto provider =
        owned(new PlugProvider(module->getFactory(), classInfo, true));
    if (provider->initialize())
      return provider;
  }
  return nullptr;
}

bool exerciseEditor(IEditController *controller, const Options &options) {
  bool ok = true;

  IPtr<IPlugView> view = owned(controller->createView(ViewType::kEditor));
  if (!check(view != nullptr, "createView(kEditor) returns a view"))
    return false;
  if (!check(view->isPlatformTypeSupported(kPlatformTypeHWND) == kResultTrue,
             "view supports kPlatformTypeHWND"))
    return false;

  ViewRect size;
  if (!check(view->getSize(&size) == kResultTrue && size.getWidth() > 0 &&
                 size.getHeight() > 0,
             "getSize reports a non-empty size"))
    return false;
  std::printf("       size: %dx%d\n", size.getWidth(), size.getHeight());

  HWND frame = createFrameWindow(size);
  if (!check(frame != nullptr, "host frame window is created"))
    return false;

  auto plugFrame = owned(new PlugFrame(frame));
  view->setFrame(plugFrame);
  if (!check(view->attached(frame, kPlatformTypeHWND) == kResultOk,
             "attached succeeds")) {
    DestroyWindow(frame);
    return false;
  }

  HWND child = GetWindow(frame, GW_CHILD);
  ok &= check(child != nullptr, "view creates a child window of the frame");
  if (child) {
    SIZE childSize = clientSize(child);
    ok &= check(childSize.cx == size.getWidth() &&
                    childSize.cy == size.getHeight(),
                "child window matches getSize");

    UpdateWindow(frame);
    pumpPendingMessages();
    auto pixels = capturePixels(child, childSize);
    std::set<DWORD> colors(pixels.begin(), pixels.end());
    ok &= check(colors.size() > 1, "view paints more than a flat background");
    if (!options.screenshotPath.empty())
      ok &= check(writeBmp(options.screenshotPath, childSize, pixels),
                  "screenshot is written");
  }

  if (view->canResize() == kResultTrue) {
    ViewRect resized{0, 0, size.getWidth() + 40, size.getHeight() + 30};
    ok &= check(plugFrame->resizeView(view, &resized) == kResultTrue,
                "onSize accepts a new size");
    if (child) {
      SIZE childSize = clientSize(child);
      ok &= check(childSize.cx == resized.getWidth() &&
                      childSize.cy == resized.getHeight(),
                  "child window follows onSize");
    }
  }

  if (options.interactive) {
    std::printf("       close the window to continue\n");
    runMessageLoop();
  }

  ok &= check(view->removed() == kResultOk, "removed succeeds");
  ok &= check(!child || !IsWindow(child), "removed destroys the child window");
  view->setFrame(nullptr);
  DestroyWindow(frame);
  pumpPendingMessages();
  return ok;
}

bool reopenEditor(IEditController *controller) {
  IPtr<IPlugView> view = owned(controller->createView(ViewType::kEditor));
  if (!view)
    return check(false, "editor can be reopened");

  ViewRect size;
  view->getSize(&size);
  HWND frame = createFrameWindow(size);
  bool attached = view->attached(frame, kPlatformTypeHWND) == kResultOk;
  if (attached)
    view->removed();
  DestroyWindow(frame);
  pumpPendingMessages();
  return check(attached, "editor can be reopened");
}

} // namespace

int main(int argc, char **argv) {
  Options options;
  if (!parseOptions(argc, argv, options)) {
    std::fprintf(stderr,
                 "usage: %s [--screenshot out.bmp] [--interactive] "
                 "plugin.vst3\n",
                 argv[0]);
    return 2;
  }

  HostApplication hostApplication;
  PluginContextFactory::instance().setPluginContext(&hostApplication);

  std::string error;
  auto module = VST3::Hosting::Module::create(options.pluginPath, error);
  if (!check(module != nullptr, "module is loaded")) {
    std::printf("       %s\n", error.c_str());
    return 1;
  }

  bool ok = false;
  {
    auto provider = loadPlugin(module);
    if (check(provider != nullptr, "audio effect is instantiated")) {
      IPtr<IEditController> controller = provider->getControllerPtr();
      if (check(controller != nullptr, "edit controller is available"))
        ok = exerciseEditor(controller, options) && reopenEditor(controller);
    }
  }
  module.reset();

  std::printf("%s\n", ok ? "OK" : "FAILED");
  return ok ? 0 : 1;
}
