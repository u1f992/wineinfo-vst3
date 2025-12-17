#pragma once
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"
#include <string>
#include <windows.h>

namespace WineInfo {

class ViewWin32 final : public Steinberg::IPlugView {
public:
  explicit ViewWin32(std::wstring text);
  virtual ~ViewWin32();

  // IPlugView
  Steinberg::tresult PLUGIN_API
  isPlatformTypeSupported(Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API attached(void *parent,
                                         Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API removed() override;
  Steinberg::tresult PLUGIN_API onWheel(float) override {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API onKeyDown(Steinberg::char16, Steinberg::int16,
                                          Steinberg::int16) override {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API onKeyUp(Steinberg::char16, Steinberg::int16,
                                        Steinberg::int16) override {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect *size) override;
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect *newSize) override;
  Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool) override {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API
  setFrame(Steinberg::IPlugFrame *frame) override {
    frame_ = frame;
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API canResize() override {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API
  checkSizeConstraint(Steinberg::ViewRect *) override {
    return Steinberg::kResultOk;
  }

  // FUnknown
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid,
                                               void **obj) override;
  Steinberg::uint32 PLUGIN_API addRef() override { return ++refCount_; }
  Steinberg::uint32 PLUGIN_API release() override {
    if (--refCount_ == 0) {
      delete this;
      return 0;
    }
    return refCount_;
  }

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

private:
  Steinberg::uint32 refCount_ = 1;
  Steinberg::IPlugFrame *frame_ = nullptr;
  HWND hwnd_ = nullptr;
  std::wstring text_;
  Steinberg::ViewRect rect_{0, 0, 520, 180};
};

} // namespace WineInfo
