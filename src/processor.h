#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"

namespace WineInfo {

class Processor final : public Steinberg::Vst::AudioEffect {
public:
  Processor();
  static Steinberg::FUnknown *createInstance(void *) {
    return (Steinberg::Vst::IAudioProcessor *)new Processor();
  }

  Steinberg::tresult PLUGIN_API
  initialize(Steinberg::FUnknown *context) override;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
  Steinberg::tresult PLUGIN_API
  process(Steinberg::Vst::ProcessData &data) override;
};

} // namespace WineInfo
