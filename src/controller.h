#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "wineinfo.h"

namespace WineInfo {

class Controller final : public Steinberg::Vst::EditControllerEx1 {
public:
  Controller() = default;
  static Steinberg::FUnknown *createInstance(void *) {
    return (Steinberg::Vst::IEditController *)new Controller();
  }

  Steinberg::tresult PLUGIN_API
  initialize(Steinberg::FUnknown *context) override;
  Steinberg::IPlugView *PLUGIN_API
  createView(Steinberg::FIDString name) override;

private:
  InfoStrings info_;
};

} // namespace WineInfo
