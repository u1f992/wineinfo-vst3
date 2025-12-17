#include "processor.h"
#include "pluginids.h"
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace WineInfo {

Processor::Processor() { setControllerClass(kControllerUID); }

tresult PLUGIN_API Processor::initialize(FUnknown *context) {
  tresult r = AudioEffect::initialize(context);
  if (r != kResultOk)
    return r;

  addAudioInput(STR16("In"), SpeakerArr::kStereo);
  addAudioOutput(STR16("Out"), SpeakerArr::kStereo);
  return kResultOk;
}

tresult PLUGIN_API Processor::setActive(TBool state) {
  return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::process(ProcessData &data) {
  if (data.numInputs == 0 || data.numOutputs == 0)
    return kResultOk;
  if (!data.inputs[0].channelBuffers32 || !data.outputs[0].channelBuffers32)
    return kResultOk;

  int32 numCh = data.outputs[0].numChannels;
  int32 n = data.numSamples;

  for (int32 ch = 0; ch < numCh; ++ch) {
    float *out = data.outputs[0].channelBuffers32[ch];
    float *in = data.inputs[0].channelBuffers32[ch];
    if (in && out)
      std::memcpy(out, in, sizeof(float) * (size_t)n);
  }
  return kResultOk;
}

} // namespace WineInfo
