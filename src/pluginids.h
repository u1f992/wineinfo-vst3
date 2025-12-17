#pragma once
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace WineInfo {

static const Steinberg::FUID kProcessorUID(0x2E3D3A01, 0x0A2B4C5D, 0x8E9F1011,
                                           0x12131415);
static const Steinberg::FUID kControllerUID(0x2E3D3A02, 0x0A2B4C5D, 0x8E9F1011,
                                            0x12131415);

enum ParamID : Steinberg::Vst::ParamID {
  kParamVersion = 0,
  kParamBuildId,
  kParamProcSelfExe,
  kParamWineloader,
  kParamWineprefix,
  kParamWineserver,
};

} // namespace WineInfo
