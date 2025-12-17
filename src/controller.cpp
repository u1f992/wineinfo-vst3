#include "controller.h"
#include "pluginids.h"
#include "wineinfo.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace WineInfo {

static void addInfoParam(ParameterContainer &params, ParamID id,
                         const wchar_t *label, const std::wstring &value) {
  std::wstring title = std::wstring(label) + value;

  String128 title128;
  size_t len = title.length();
  if (len > 127)
    len = 127;
  for (size_t i = 0; i < len; ++i) {
    title128[i] = static_cast<char16_t>(title[i]);
  }
  title128[len] = 0;

  params.addParameter(title128, nullptr, 0, 0, ParameterInfo::kIsReadOnly, id);
}

tresult PLUGIN_API Controller::initialize(FUnknown *context) {
  tresult r = EditControllerEx1::initialize(context);
  if (r != kResultOk)
    return r;

  auto info = get_info_strings();

  addInfoParam(parameters, kParamVersion, L"Wine version: ", info.version);
  addInfoParam(parameters, kParamBuildId, L"Wine build-id: ", info.build_id);
  addInfoParam(parameters, kParamProcSelfExe, L"/proc/self/exe: ",
               info.proc_self_exe);
  addInfoParam(parameters, kParamWineloader, L"WINELOADER: ", info.wineloader);
  addInfoParam(parameters, kParamWineprefix, L"WINEPREFIX: ", info.wineprefix);
  addInfoParam(parameters, kParamWineserver, L"WINESERVER: ", info.wineserver);

  return kResultOk;
}

IPlugView *PLUGIN_API Controller::createView(FIDString /*name*/) {
  return nullptr;
}

} // namespace WineInfo
